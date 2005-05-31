/*
 * CAPI interface handling
 *
 * heavily based on capiplugin.c of pppdcapiplugin by
 * Carsten Paeth (calle@calle.in-berlin.de)
 *
 * Copyright 2004 SYSGO Real-Time Solutions AG
 * Klein-Winternheim, Germany
 * All rights reserved.
 *
 * Author: Armin Schindler <armin.schindler@sysgo.com>
 *
 * $Id$
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "isdntun.h"
#include "capiconn.h"
#include "options.h"
#include <malloc.h>
#include <string.h>
#include <errno.h>


/* -------------------------------------------------------------------- */

static int exiting = 0;

/* -------------------------------------------------------------------- */

typedef struct stringlist {
    struct stringlist *next;
	char              *s;
} STRINGLIST;

/* -------------------------------------------------------------------- */

static int curphase = -1;
static int wakeupnow = 0;

/* -------------------------------------------------------------------- */

static void handlemessages(void) ;
static int shmatch(char *string, char *expr);
static void stringlist_free(STRINGLIST **pp);
static int stringlist_append_string(STRINGLIST **pp, char *s);
static STRINGLIST *stringlist_split(char *tosplit, char *seps);
static void makecallback(void);
static char *phase2str(int phase);
static void wakeupdemand(void);

/* -------------------------------------------------------------------- */

static capiconn_context *ctx;
static unsigned applid;
#define CM(x)	(1<<(x))
#define	CIPMASK_ALL	0x1FFF03FF
#define	CIPMASK_VOICE	(CM(1)|CM(4)|CM(5)|CM(16)|CM(26))
#define	CIPMASK_DATA	(CM(2)|CM(3))
static unsigned long cipmask = CIPMASK_ALL;
static int controller = 1;
static capi_contrinfo cinfo = { 0 , 0, 0 };

/* -------------------------------------------------------------------- */

/*
 * numbers
 */
static STRINGLIST *numbers;
static STRINGLIST *callbacknumbers;
static STRINGLIST *clis;
static STRINGLIST *inmsns;
static STRINGLIST *controllers;

/*
 * protocol
 */
#define PROTO_HDLC		0
#define PROTO_X75		1
static int proto = PROTO_HDLC;

/*
 * leased line
 */
static unsigned char AdditionalInfo[1+2+2+31];

/*
 * callback & COSO
 */
#define COSO_CALLER	0
#define COSO_LOCAL	1
#define COSO_REMOTE	2
static int coso = COSO_CALLER;

/* -------------------------------------------------------------------- */
/* -------- Handle leased lines (CAPI-Bundling) ----------------------- */
/* -------------------------------------------------------------------- */

static int decodechannels(char *teln, unsigned long *bmaskp, int *activep)
{
	unsigned long bmask = 0;
	int active = !0;
	char *s;
	int channel;
	int i;

	s = teln;
	while (*s && *s == ' ') s++;
	if (!*s)
		fatal("option channels: list empty");
	if (*s == 'p' || *s == 'P') {
		active = 0;
		s++;
	}
	if (*s == 'a' || *s == 'A') {
		active = !0;
		s++;
	}
	while (*s) {
		int digit1 = 0;
		int digit2 = 0;
		if (!isdigit(*s))
			goto illegal;
		while (isdigit(*s)) { digit1 = digit1*10 + (*s - '0'); s++; }
		channel = digit1;
		if (channel <= 0 && channel > 30)
			goto rangeerror;
		if (*s == 0 || *s == ',' || *s == ' ') {
			bmask |= (1 << digit1);
			digit1 = 0;
			if (*s) s++;
			continue;
		}
		if (*s != '-') 
			goto illegal;
		s++;
		if (!isdigit(*s)) return -3;
		while (isdigit(*s)) { digit2 = digit2*10 + (*s - '0'); s++; }
		channel = digit2;
		if (channel <= 0 && channel > 30)
			goto rangeerror;
		if (*s == 0 || *s == ',' || *s == ' ') {
			if (digit1 > digit2)
				for (i = digit2; i <= digit1 ; i++)
					bmask |= (1 << i);
			else 
				for (i = digit1; i <= digit2 ; i++)
					bmask |= (1 << i);
			digit1 = digit2 = 0;
			if (*s) s++;
			continue;
		}
		goto illegal;
	}
	if (activep) *activep = active;
	if (bmaskp) *bmaskp = bmask;
	return 0;
illegal:
	fatal("option channels: illegal octet '%c'", *s);
	return -1;
rangeerror:
	fatal("option channels: channel %d out of range", channel);
	return -1;
}

static int channels2capi20(char *teln, unsigned char *AdditionalInfo)
{
	unsigned long bmask;
	int active;
	int i;
   
	decodechannels(teln, &bmask, &active);
	/* info("\"%s\" 0x%lx %d\n", teln, bmask, active); */
	/* Length */
	AdditionalInfo[0] = 2+2+31;
        /* Channel: 3 => use channel allocation */
        AdditionalInfo[1] = 3; AdditionalInfo[2] = 0;
	/* Operation: 0 => DTE mode, 1 => DCE mode */
        if (active) {
   		AdditionalInfo[3] = 0; AdditionalInfo[4] = 0;
   	} else {
   		AdditionalInfo[3] = 1; AdditionalInfo[4] = 0;
	}
	/* Channel mask array */
	AdditionalInfo[5] = 0; /* no D-Channel */
	for (i=1; i <= 30; i++)
		AdditionalInfo[5+i] = (bmask & (1 << i)) ? 0xff : 0;
	return 0;
}



/* -------------------------------------------------------------------- */

static void itun_check_options(void)
{
	static int init = 0;

	if (init)
		return;
	init = 1;

	/*
	 * protocol
	 */
	if (strcasecmp(opt_proto, "hdlc") == 0) {
	   proto = PROTO_HDLC;
	} else if (strcasecmp(opt_proto, "x75") == 0) {
	   proto = PROTO_X75;
	} else {
	   option_error("unknown protocol \"%s\"", opt_proto);
	   die(1);
	}
	cipmask = CIPMASK_DATA;

	/*
	 * coso
	 */
	if (opt_coso == 0) {
	   if (opt_cbflag) {
	      if (opt_number) coso = COSO_REMOTE;
	      else coso = COSO_LOCAL;
	   } else {
	      coso = COSO_CALLER;
	   }
	} else if (strcasecmp(opt_coso, "caller") == 0) {
	   coso = COSO_CALLER;
	} else if (strcasecmp(opt_coso, "local") == 0) {
	   coso = COSO_LOCAL;
	} else if (strcasecmp(opt_coso, "remote") == 0) {
	   coso = COSO_REMOTE;
	} else {
	    option_error("wrong value for option coso");
	    die(1);
	}
	if (opt_cbflag) {
	   if (opt_coso) {
	      option_error("option clicb ignored");
	   } else if (coso == COSO_REMOTE) {
	      dbglog("clicb selected coso remote");
	   } else if (coso == COSO_LOCAL) {
	      dbglog("clicb selected coso local");
	   } else {
	      option_error("option clicb ignored");
	   }
	}

	/*
	 * leased line
	 */
	if (opt_channels) {
		channels2capi20(opt_channels, AdditionalInfo);
		if (opt_number)
			option_error("option number ignored");
		if (opt_numberprefix)
			option_error("option numberprefix ignored");
		if (opt_callbacknumber)
			option_error("option callbacknumber ignored");
		if (opt_msn)
			option_error("option msn ignored");
		if (opt_inmsn)
			option_error("option inmsn ignored");
	/*
	 * dialout
	 */
	} else if (opt_number) {
		stringlist_free(&numbers);
		numbers = stringlist_split(opt_number, " \t,");
	/*
	 * dialin
	 */
	} else {
	   if (coso == COSO_LOCAL) {
	      if (opt_callbacknumber == 0) {
		 option_error("option callbacknumber missing");
		 die(1);
	       }
	   }
	}

	if (opt_callbacknumber) {
	   stringlist_free(&callbacknumbers);
	   callbacknumbers = stringlist_split(opt_callbacknumber, " \t,");
	}

	/*
	 * ddi
	 */
	memset(&cinfo, 0, sizeof(cinfo));
	if (opt_ddi) {
		STRINGLIST *parsed_ddi;
		STRINGLIST *sl;
		char *tmp;
		parsed_ddi = stringlist_split(opt_ddi, " \t,");
		sl = parsed_ddi;
		cinfo.ddi = strdup(sl->s);
		if (cinfo.ddi == 0) {
			stringlist_free(&parsed_ddi);
			option_error("option ddi error");
			die(1);
		}
		if (sl->next && sl->next->s) {
			sl = sl->next;
			cinfo.ndigits = strtol(sl->s, &tmp, 10);
			if (tmp == sl->s || *tmp) {
				stringlist_free(&parsed_ddi);
				option_error("option ddi error");
				die(1);
			}
		}
		stringlist_free(&parsed_ddi);
	}

	/*
	 * controller
	 */
	if (opt_controller) {
		STRINGLIST *sl;
		int count = 0, contr;
		stringlist_free(&controllers);
		controllers = stringlist_split(opt_controller, " \t,");
		for (sl = controllers; sl; sl = sl->next) {
			count++;
			contr = strtol(sl->s, NULL, 10);
			if ((contr < 1) || (contr > 30)) {
				option_error("illegal controller specification \"%s\"",
					opt_controller);
				die(1);
			}
			if (count == 1)
				controller = contr;
		}
		if (count < 2) {
			stringlist_free(&controllers);
		}
	}
		
	/*
	 * cli & inmsn
	 */
	if (opt_cli) {
		STRINGLIST *sl;
		char *old;
		stringlist_free(&clis);
		clis = stringlist_split(opt_cli, " \t,");
		for (sl = clis; sl; sl = sl->next) {
		   if (*sl->s != '*') {
		      old = sl->s;
		      sl->s = (char *)malloc(strlen(old)+2);
		      if (sl->s) {
			 *sl->s = '*';
			 strcpy(sl->s+1, old);
			 free(old);
		      } else {
			 sl->s = old;
	                 option_error("prepend '*' to cli failed");
		      }
		   }
		}
	}
	if (opt_inmsn) {
		stringlist_free(&inmsns);
		inmsns = stringlist_split(opt_inmsn, " \t,");
	}

	/*
	 * dial on demand
	 */
	if (demand) {
	   if (!opt_number && !opt_channels) {
	       option_error("number or channels missing for demand");
	       die(1);
	   }
	   if (opt_voicecallwakeup)
	      cipmask |= CIPMASK_VOICE;
	} else if (opt_voicecallwakeup) {
	   option_error("option voicecallwakeup ignored");
	   opt_voicecallwakeup = 0;
	}

	return;
}

/* -------------------------------------------------------------------- */
/* -------- Match with * and ? ---------------------------------------- */
/* -------------------------------------------------------------------- */

static int shmatch(char *string, char *expr)
{
   char *match = expr;
   char *s = string;
   char *p;
   int escape = 0;

   while (*match && *s) {
      if (escape) {
	     if (*s != *match)
		    return 0;
	     s++;
		 match++;
      } else if (*match == '\\') {
         match++;
         escape = 1;
      } else if (*match == '*') {
		 match++;
		 if (*match == 0) 
		    return 1;
         if (*match == '\\') 
            match++;
         while ((p = strchr(s, *match)) != 0) {
		    if (shmatch(p+1, match+1))
			   return 1;
		    s = p + 1;
         }
		 return 0;
	  } else if (*match == '?') {
	     s++;
		 match++;
	  } else {
	     if (*s != *match)
		    return 0;
	     s++;
		 match++;
	  }
   }
   if (*s == 0) {
      if (*match == 0) return 1;
      if (*match == '*' && match[1] == 0) return 1;
   }
   return 0;
}

/* -------------------------------------------------------------------- */
/* -------- STRINGLIST for parsing ------------------------------------ */
/* -------------------------------------------------------------------- */

static void stringlist_free(STRINGLIST **pp)
{
	STRINGLIST *p, *next;

	p = *pp;
	while (p) {
		next = p->next;
  		if (p->s) free(p->s);
  		free(p);
  		p = next; 
	}
	*pp = 0;
}

static int stringlist_append_string(STRINGLIST **pp, char *s)
{
	STRINGLIST *p;
	for (; *pp; pp = &(*pp)->next) ;
	if ((p = (STRINGLIST *)malloc(sizeof(STRINGLIST))) == 0)
		return -1;
	memset(p, 0, sizeof(STRINGLIST));
	if ((p->s = strdup(s)) == 0) {
		free(p);
		return -1;
	}
	p->next = 0;
	*pp = p;
	return 0;
}

static STRINGLIST *stringlist_split(char *tosplit, char *seps)
{
	STRINGLIST *p = 0;
	char *str = strdup(tosplit);
	char *s;
	if (!str) return 0;
	for (s = strtok(str, seps); s; s = strtok(0, seps)) {
		if (*s == 0) continue; /* if strtok is not working correkt */
		if (stringlist_append_string(&p, s) < 0) {
 			stringlist_free(&p);
 			free(str);
 			return 0;
		}
	}
	free(str);
	return p;
}

/* -------------------------------------------------------------------- */
/* -------- connection management ------------------------------------- */
/* -------------------------------------------------------------------- */

typedef struct conn {
    struct conn     *next;
    capi_connection *conn;
    int              type;
#define CONNTYPE_OUTGOING	0
#define CONNTYPE_INCOMING	1
#define CONNTYPE_IGNORE		2
#define CONNTYPE_REJECT		3
#define CONNTYPE_FOR_CALLBACK	4
    int              inprogress;    
    int              isconnected;    
} CONN;

static CONN *connections;

/* -------------------------------------------------------------------- */

static CONN *conn_remember(capi_connection *conn, int type)
{
	CONN *p, **pp;
	for (pp = &connections; *pp; pp = &(*pp)->next) ;
	if ((p = (CONN *)malloc(sizeof(CONN))) == 0) {
       		int serrno = errno;
       		fatal("malloc failed - %s (%d)",
		   	strerror(serrno), serrno);
		return 0;
	}
	memset(p, 0, sizeof(CONN));
	p->conn = conn;
	p->type = type;
	p->next = 0;
	switch (type) {
           case CONNTYPE_OUTGOING:
           case CONNTYPE_INCOMING:
           case CONNTYPE_FOR_CALLBACK:
	      p->inprogress = 1;
	      p->isconnected = 0;
	      break;
	   default:
	      break;
	}
	*pp = p;
	return p;
}

static int conn_forget(capi_connection *conn)
{
	CONN **pp, *p;
	for (pp = &connections; *pp && (*pp)->conn != conn; pp = &(*pp)->next) ;
	if (*pp) {
	   p = *pp;
	   *pp = (*pp)->next;
	   free(p);
	   return 0;
        }
	return -1;
}

static CONN *conn_find(capi_connection *cp)
{
	CONN *p;
	for (p = connections; p; p = p->next) {
	   if (p->conn == cp)
	      return p;
	}
	return 0;
}

/* -------------------------------------------------------------------- */

static int conn_connected(capi_connection *conn)
{
	CONN *p;
	for (p = connections; p; p = p->next) {
	   if (p->conn == conn) {
	      p->isconnected = 1;
	      p->inprogress = 0;
	      return 0;
	   }
	}
	fatal("connected connection not found ??");
	return -1;
}

/* -------------------------------------------------------------------- */

static int conn_incoming_connected(void)
{
    CONN *p;
    for (p = connections; p; p = p->next) {
       if (p->type == CONNTYPE_INCOMING)
	  return p->isconnected;    
    }
    return 0;
}

static int conn_incoming_exists(void)
{
    CONN *p;
    for (p = connections; p; p = p->next) {
       if (p->type == CONNTYPE_INCOMING)
	  return p->isconnected || p->inprogress;    
    }
    return 0;
}

/* -------------------------------------------------------------------- */

static int conn_inprogress(capi_connection *cp)
{
    CONN *p;
    for (p = connections; p; p = p->next) {
       if (p->conn == cp)
	  return p->inprogress;    
    }
    return 0;
}

static int conn_isconnected(capi_connection *cp)
{
    CONN *p;
    if (cp) {
       for (p = connections; p; p = p->next) {
          if (p->conn == cp)
	     return p->isconnected;    
       }
    } else {
       for (p = connections; p; p = p->next) {
	  if (p->isconnected)
	     return 1;
       }
    }
    return 0;
}

/* -------------------------------------------------------------------- */
/* -------------------------------------------------------------------- */
/* -------------------------------------------------------------------- */

static int act_controller = 1;

static void reset_contr_status(void)
{
	act_controller = 1;
}

static int get_next_contr(void)
{
	STRINGLIST *sl;
	int contr, count = 0;

	if (!controllers) {
		/* one controller only */
		return(controller);
	}

	for (sl = controllers; sl; sl = sl->next) {
		count++;
		contr = strtol(sl->s, NULL, 10);
		if (count == act_controller) {
			act_controller++;
			return(contr);
		}
	}
	act_controller = 1;
	return(strtol(controllers->s, NULL, 10));
}

static void do_listen(unsigned cipmask, unsigned cipmask2)
{
	STRINGLIST *sl;
	int contr;
	
	if (!controllers) {
		/* one controller only */
		(void) capiconn_listen(ctx, controller, cipmask, cipmask2);
		return;
	}

	for (sl = controllers; sl; sl = sl->next) {
		contr = strtol(sl->s, NULL, 10);
		(void) capiconn_listen(ctx, contr, cipmask, cipmask2);
	}
}

/* -------------------------------------------------------------------- */
/* -------- demand & wakeup ------------------------------------------- */
/* -------------------------------------------------------------------- */

static void setupincoming_for_demand(void)
{
	do_listen(cipmask, 0);
	info("waiting for demand wakeup ...");
}

static void wakeupdemand(void)
{
    if (curphase != PHASE_DORMANT) {
       info("wakeup not possible in phase %s, delayed",
		phase2str(curphase));
       wakeupnow++;
       return;
    }
    capi_wake_up();
}

/* -------------------------------------------------------------------- */
/* -------- CAPI setup & handling ------------------------------------- */
/* -------------------------------------------------------------------- */

static void init_capiconn(void)
{
	static int init = 0;
	STRINGLIST *sl;
	int contr;

	if (init) {
		do_listen(0, 0);
		return;
	}
	init = 1;

	if (!controllers) {
		/* one controller only */
		if (capiconn_addcontr(ctx, controller, &cinfo) != CAPICONN_OK) {
			(void)capiconn_freecontext(ctx);
			(void)capi20_release(applid);
			fatal("add controller %d failed", controller);
			return;
		}
		if (cinfo.ddi) 
			dbglog("contr=%d ddi=\"%s\" n=%d",
				controller, cinfo.ddi, cinfo.ndigits);
		else
			dbglog("contr=%d", controller);
	} else {
		for (sl = controllers; sl; sl = sl->next) {
			contr = strtol(sl->s, NULL, 10);
			if (capiconn_addcontr(ctx, contr, &cinfo) != CAPICONN_OK) {
				(void)capiconn_freecontext(ctx);
				(void)capi20_release(applid);
				fatal("add controller %d failed", contr);
				return;
			}
			if (cinfo.ddi) 
				dbglog("contr=%d ddi=\"%s\" n=%d",
					contr, cinfo.ddi, cinfo.ndigits);
			else
				dbglog("contr=%d", contr);
		}
	}

	do_listen(0, 0);
}

/* -------------------------------------------------------------------- */

void handle_capi_message(void) 
{
	unsigned char *msg = 0;
	if (capi20_get_message(applid, &msg) == 0)
		capiconn_inject(applid, msg);
}

static void handlemessages(void)
{
	unsigned char *msg = 0;
	struct timeval tv;
	tv.tv_sec  = 1;
	tv.tv_usec = 0;
	if (capi20_waitformessage(applid, &tv) == 0) {
		if (capi20_get_message(applid, &msg) == 0)
			capiconn_inject(applid, msg);
	}
}

/* -------------------------------------------------------------------- */

static void dodisconnect(capi_connection *cp)
{
	CONN *conn;
	time_t t;

	if ((conn = conn_find(cp)) == 0)
		return;
	(void)capiconn_disconnect(cp, 0);
	conn->isconnected = conn->inprogress = 0;

	t = time(0)+10;
	do {
	    handlemessages();
        } while (conn_find(cp) && time(0) < t);

        if (conn_find(cp))
		fatal("timeout while waiting for disconnect");
}

static void disconnectall(void)
{
	time_t t;
	CONN *p;

        do_listen(0, 0);
	for (p = connections; p; p = p->next) {
	      if (p->inprogress || p->isconnected) {
		 p->isconnected = p->inprogress = 0;
        	 capiconn_disconnect(p->conn, 0);
	      }
	}
	t = time(0)+10;
	do {
	    handlemessages();
        } while (connections && time(0) < t);

	if (connections && !exiting)
        	fatal("disconnectall failed");
}

/* -------------------------------------------------------------------- */
/* -------------------------------------------------------------------- */

static char *conninfo(capi_connection *p)
{
	static char buf[1024];
	capi_conninfo *cp = capiconn_getinfo(p);
	char *callingnumber = "";
	char *callednumber = "";

	if (cp->callingnumber && cp->callingnumber[0] > 2)
		callingnumber = cp->callingnumber+3;
	if (cp->callednumber && cp->callednumber[0] > 1)
		callednumber = cp->callednumber+2;

	snprintf(buf, sizeof(buf),
		"\"%s\" -> \"%s\" %s (pcli=0x%x/ncci=0x%x)",
			callingnumber, callednumber,
			cp->isincoming ? "incoming" : "outgoing",
			cp->plci, cp->ncci
			);
	buf[sizeof(buf)-1] = 0;
	return buf;
}

/* -------------------------------------------------------------------- */
/* -------- reject reason handling (wuerg) ---------------------------- */
/* -------------------------------------------------------------------- */

static unsigned dreason = 0;

static int was_no_reject(void)
{
      if ((dreason & 0xff00) != 0x3400)
	 return 1;
      switch (dreason) {
	 case 0x34a2: /* No circuit / channel available */
	    return 1;
      }
      return 0;
}

/* -------------------------------------------------------------------- */
/* -------- disconnect handler ---------------------------------------- */
/* -------------------------------------------------------------------- */

static void disconnected(capi_connection *cp,
				int localdisconnect,
				unsigned reason,
				unsigned reason_b3)
{
	CONN *p;

	if ((p = conn_find(cp)) == 0)
	   return;
	   
        conn_forget(cp);

	capi_is_disconnected(cp);
	
	switch (p->type) {
           case CONNTYPE_OUTGOING:
           case CONNTYPE_INCOMING:
	      break;
           case CONNTYPE_IGNORE:
           case CONNTYPE_REJECT:
	      return;
           case CONNTYPE_FOR_CALLBACK:
	      dreason = reason;
              break;

	}
	if (reason != 0x3304) /* Another Applikation got the call */
		info("disconnect(%s): %s 0x%04x (0x%04x) - %s", 
			localdisconnect ? "local" : "remote",
			conninfo(cp),
			reason, reason_b3, capi_info2str(reason));
}

/* -------------------------------------------------------------------- */
/* -------- incoming call handler ------------------------------------- */
/* -------------------------------------------------------------------- */

static void incoming(capi_connection *cp,
				unsigned contr,
				unsigned cipvalue,
				char *callednumber,
				char *callingnumber)
{
	STRINGLIST *p;
        char *s;

	info("incoming call: %s (0x%x)", conninfo(cp), cipvalue);

        if (conn_incoming_exists()) {
	   info("ignoring call, incoming connection exists");
           conn_remember(cp, CONNTYPE_IGNORE);
	   (void) capiconn_ignore(cp);
           return;
	}
	
	if (opt_inmsn) {
	   for (p = inmsns; p; p = p->next) {
	      if (   (s = strstr(callednumber, p->s)) != 0
                  && strcmp(s, p->s) == 0)
		 break;
	   }
	   if (!p) {
	           info("ignoring call, msn %s not in \"%s\"",
				callednumber, opt_inmsn);
                   conn_remember(cp, CONNTYPE_IGNORE);
		   (void) capiconn_ignore(cp);
		   return;
           }
        } else if (opt_msn) {
	   if (   (s = strstr(callednumber, opt_msn)) == 0
               || strcmp(s, opt_msn) != 0) {
	           info("ignoring call, msn mismatch (%s != %s)",
				opt_msn, callednumber);
                   conn_remember(cp, CONNTYPE_IGNORE);
		   (void) capiconn_ignore(cp);
		   return;
           }
        }

	if (opt_cli) {
	   for (p = clis; p; p = p->next) {
	       if (shmatch(callingnumber, p->s))
		  break;
	   }
	   if (!p) {
	           info("ignoring call, cli mismatch (%s != %s)",
				opt_cli, callingnumber);
                   conn_remember(cp, CONNTYPE_IGNORE);
		   (void) capiconn_ignore(cp);
		   return;
	   }
        } else if (opt_number) {
	   for (p = numbers; p; p = p->next) {
	       if (   (s = strstr(callingnumber, p->s)) != 0
                   && strcmp(s, p->s) == 0)
		   break;
           }
	   if (!p) {
	           info("ignoring call, number mismatch (%s != %s)",
				opt_number, callingnumber);
                   conn_remember(cp, CONNTYPE_IGNORE);
		   (void) capiconn_ignore(cp);
		   return;
	   }
        } else if (opt_acceptdelayflag) {
		/*
		* non cli or number match,
		* give more specific listen a chance (bad)
		*/
		info("accept delayed, no cli or number match");
		sleep(1);
	}

	switch (cipvalue) {
		case 1:	 /* Speech */
		case 4:  /* 3.1 kHz audio */
		case 5:  /* 7 kHz audio */
		case 16: /* Telephony */
		case 26: /* 7 kHz telephony */
                        if (opt_voicecallwakeup) {
			   goto wakeupdemand;
			} else {
	                   info("ignoring speech call from %s",
			   		callingnumber);
                           conn_remember(cp,CONNTYPE_IGNORE);
			   (void) capiconn_ignore(cp);
			}
			break;
		case 2:  /* Unrestricted digital information */
		case 3:  /* Restricted digital information */
	                if (proto == PROTO_HDLC) {
			   if (demand) goto wakeupmatch;
			   if (coso == COSO_LOCAL) goto callback;
			   goto accept;
	                } else if (proto == PROTO_X75) {
			   if (demand) goto wakeupmatch;
			   if (coso == COSO_LOCAL) goto callback;
			   goto accept;
			} else {
	                   info("ignoring digital call from %s",
			   		callingnumber);
                           conn_remember(cp,CONNTYPE_IGNORE);
			   (void) capiconn_ignore(cp);
			}
			break;
		case 17: /* Group 2/3 facsimile */
		        info("ignoring fax call from %s",
			   		callingnumber);
                        conn_remember(cp,CONNTYPE_IGNORE);
			(void) capiconn_ignore(cp);
			break;
		default:
		        info("ignoring type %d call from %s",
			   		cipvalue, callingnumber);
                        conn_remember(cp,CONNTYPE_IGNORE);
			(void) capiconn_ignore(cp);
			break;
	}
	return;

callback:
	do_listen(0, 0);
	dbglog("rejecting call: %s (0x%x)", conninfo(cp), cipvalue);
	conn_remember(cp, CONNTYPE_REJECT);
	capiconn_reject(cp);
        makecallback();
	return;

wakeupdemand:
	dbglog("rejecting call: %s (0x%x)", conninfo(cp), cipvalue);
	conn_remember(cp, CONNTYPE_REJECT);
	capiconn_reject(cp);
        wakeupdemand();
	return;

wakeupmatch:
	if (coso == COSO_LOCAL)
	   goto callback;

accept:
	switch (proto) {
	   default:
	   case PROTO_HDLC:
	      (void) capiconn_accept(cp, 0, 1, 0, 0, 0, 0, 0);
	      break;
	   case PROTO_X75:
	      (void) capiconn_accept(cp, 0, 0, 0, 0, 0, 0, 0);
	      break;
	}
	conn_remember(cp, CONNTYPE_INCOMING);
	do_listen(0, 0);
	return;
}

/* -------------------------------------------------------------------- */
/* -------- connection established handler ---------------------------- */
/* -------------------------------------------------------------------- */

static void connected(capi_connection *cp, _cstruct NCPI)
{
        info("connected: %s", conninfo(cp));

        if (conn_incoming_connected()) {
	   time_t t = time(0)+opt_connectdelay;
	   do {
	      handlemessages();
	      if (status != EXIT_OK)
		 die(status);
	   } while (time(0) < t);
	}

	capi_is_connected(cp);

	conn_connected(cp);
	if (curphase == PHASE_DORMANT)
           wakeupdemand();
}

/* -------------------------------------------------------------------- */
/* -------- charge information ---------------------------------------- */
/* -------------------------------------------------------------------- */

void chargeinfo(capi_connection *cp, unsigned long charge, int inunits)
{
	if (inunits) {
        	info("%s: charge in units: %d", conninfo(cp), charge);
	} else {
        	info("%s: charge in currency: %d", conninfo(cp), charge);
	}
}

/* -------------------------------------------------------------------- */
/* -------- tranfer capi message to CAPI ------------------------------ */
/* -------------------------------------------------------------------- */

void put_message(unsigned appid, unsigned char *msg)
{
	unsigned err;
	err = capi20_put_message (appid, msg);
	if (err && !exiting)
		fatal("putmessage(appid=%d) - %s 0x%x",
			      appid, capi_info2str(err), err);
}

/* -------------------------------------------------------------------- */
/* -------- capiconn module setup ------------------------------------- */
/* -------------------------------------------------------------------- */

capiconn_callbacks callbacks = {
	malloc: malloc,
	free: free,

	disconnected: disconnected,
	incoming: incoming,
	connected: connected,
	received: handle_capi_data, 
	datasent: handle_capi_sent, 
	chargeinfo: chargeinfo,

	capi_put_message: put_message,

	debugmsg: (void (*)(const char *, ...))dbglog,
	infomsg: (void (*)(const char *, ...))info,
	errmsg: (void (*)(const char *, ...))error
};

/* -------------------------------------------------------------------- */
/* -------- create a connection --------------------------------------- */
/* -------------------------------------------------------------------- */

static capi_connection *setupconnection(int contr, char *num, int awaitingreject)
{
	struct capi_connection *cp;
	char number[256];

	snprintf(number, sizeof(number), "%s%s", 
			opt_numberprefix ? opt_numberprefix : "", num);

	if (proto == PROTO_HDLC) {
		cp = capiconn_connect(ctx,
				contr, /* contr */
				2, /* cipvalue */
				opt_channels ? 0 : number, 
				opt_channels ? 0 : opt_msn,
				0, 1, 0,
				0, 0, 0,
				opt_channels ? AdditionalInfo : 0,
				0);
	} else if (proto == PROTO_X75) {
		cp = capiconn_connect(ctx,
				contr, /* contr */
				2, /* cipvalue */
				opt_channels ? 0 : number, 
				opt_channels ? 0 : opt_msn,
				0, 0, 0,
				0, 0, 0,
				opt_channels ? AdditionalInfo : 0,
				0);
	} else {
		fatal("unknown protocol \"%s\"", opt_proto);
		return 0;
	}
	if (opt_channels) {
		info("leased line (%s)", opt_proto);
	} else if (awaitingreject) {
		info("dialing %s (awaiting reject)", number);
	} else {
		info("dialing %s (%s)", number, opt_proto);
	}
        if (awaitingreject)
           conn_remember(cp, CONNTYPE_FOR_CALLBACK);
	else
           conn_remember(cp, CONNTYPE_OUTGOING);
        return cp;
}

/* -------------------------------------------------------------------- */
/* -------- connect leased line --------------------------------------- */
/* -------------------------------------------------------------------- */

static void makeleasedline(void)
{
	capi_connection *cp;
	int retry = 0;
	time_t t;

	reset_contr_status();

	do {
	     if (retry) {
		t = time(0)+opt_redialdelay;
		do {
		   handlemessages();
		   if (status != EXIT_OK)
		      die(status);
		} while (time(0) < t);
	     }

	     cp = setupconnection(get_next_contr(), "", 0);

	     t = time(0)+opt_dialtimeout;
	     do {
		handlemessages();
		if (status != EXIT_OK) {
		   if (conn_find(cp)) {
		      info("status %d, disconnecting ...", status);
		      dodisconnect(cp);
		   } else {
		      die(status);
		   }
		}
	     } while (time(0) < t && conn_inprogress(cp));

	     if (conn_isconnected(cp))
		goto connected;

	     if (status != EXIT_OK)
		die(status);

	} while (++retry < opt_dialmax);

connected:
        if (conn_isconnected(cp)) {
	   t = time(0)+opt_connectdelay;
	   do {
	      handlemessages();
	      if (status != EXIT_OK)
		 die(status);
	   } while (time(0) < t);
	}

	if (status != EXIT_OK)
		die(status);

        if (!conn_isconnected(cp))
	   fatal("couldn't make connection");
}

/* -------------------------------------------------------------------- */
/* -------- connect a dialup connection ------------------------------- */
/* -------------------------------------------------------------------- */

static void makeconnection(STRINGLIST *numbers)
{
	capi_connection *cp = 0;
	time_t t;
	STRINGLIST *p;
	int retry = 0;

	reset_contr_status();

	do {
	   for (p = numbers; p; p = p->next) {
		   if (retry || p != numbers) {
		      t = time(0)+opt_redialdelay;
		      do {
			 handlemessages();
			 if (status != EXIT_OK)
			    die(status);
		      } while (time(0) < t);
		   }

		   cp = setupconnection(get_next_contr(), p->s, 0);

		   t = time(0)+opt_dialtimeout;
		   do {
		      handlemessages();
		      if (status != EXIT_OK) {
			 if (conn_find(cp)) {
			    info("status %d, disconnecting ...", status);
			    dodisconnect(cp);
			 } else {
			    die(status);
			 }
		      }
		   } while (time(0) < t && conn_inprogress(cp));

		   if (conn_isconnected(cp))
		      goto connected;

		   if (status != EXIT_OK)
		      die(status);
	   }
	} while (++retry < opt_dialmax);

connected:
        if (conn_isconnected(cp)) {
	   t = time(0)+opt_connectdelay;
	   do {
	      handlemessages();
	      if (status != EXIT_OK)
		 die(status);
	   } while (time(0) < t);
	}

        if (!conn_isconnected(cp))
	   error("couldn't make connection after %d retries",
			retry);
}

/* -------------------------------------------------------------------- */
/* -------- dial and wait for callback -------------------------------- */
/* -------------------------------------------------------------------- */

static void makeconnection_with_callback(void)
{
	capi_connection *cp;
	STRINGLIST *p;
	time_t t;
	int retry = 0;

	reset_contr_status();

	do {
   	   for (p = numbers; p; p = p->next) {

		if (retry || p != numbers) {
again:
		   t = time(0)+opt_redialdelay;
		   do {
		      handlemessages();
		      if (status != EXIT_OK)
			 die(status);
		   } while (time(0) < t);
		}

		cp = setupconnection(get_next_contr(), p->s, 1);

		/* Wait specific time for the server rejecting the call */
		t = time(0)+opt_dialtimeout;
		do {
		      handlemessages();
		      if (status != EXIT_OK)
			 die(status);
	        } while (time(0) < t && conn_inprogress(cp));

		if (conn_inprogress(cp)) {
			(void)capiconn_disconnect(cp, 0);
			dreason = 0x3490;
		}

		if (conn_isconnected(cp)) {
			dodisconnect(cp);
			error("callback failed - other party answers the call (no reject)");
	        } else if (was_no_reject()) {
	        	goto again;
		} else {
			do_listen(cipmask, 0);
			info("waiting for callback...");

			/* Wait for server calling back */
			t = time(0)+opt_cbwait;
			do {
				handlemessages();
				if (status != EXIT_OK) {
				   do_listen(0, 0);
				   die(status);
				}
			} while (!conn_incoming_connected() && time(0) < t);

                        if (conn_incoming_connected()) {
				return;
			}
			info("callback failed (no call)");
		}
	   }
	} while (++retry < opt_dialmax);

	error("callback failed (no call)");
}

/* -------------------------------------------------------------------- */
/* -------- execute a callback ---------------------------------------- */
/* -------------------------------------------------------------------- */

static void makecallback(void)
{
	time_t t;

	t = time(0)+opt_cbdelay;
	do {
	      handlemessages();
	      if (status != EXIT_OK)
		 die(status);
	} while (time(0) < t);

	if (callbacknumbers) 
	   makeconnection(callbacknumbers);
	else makeconnection(numbers);
}

/* -------------------------------------------------------------------- */
/* -------- wait for an incoming call --------------------------------- */
/* -------------------------------------------------------------------- */

static void waitforcall(void)
{
	do_listen(cipmask, 0);
	info("waiting for incoming call ...");
}

/* -------------------------------------------------------------------- */
/* -------- PPPD state change hook ------------------------------------ */
/* -------------------------------------------------------------------- */

static char *phase2str(int phase)
{
	static struct tmpbuf {
		struct tmpbuf *next;
		char           buf[32];
	} buffer[] = {
		{ &buffer[1] },
		{ &buffer[2] },
		{ &buffer[0] },
	};
	static struct tmpbuf *p = &buffer[0];

	switch (phase) {
		case PHASE_DEAD: return "dead";
		case PHASE_INITIALIZE: return "initialize";
		case PHASE_SERIALCONN: return "serialconn";
		case PHASE_DORMANT: return "dormant";
		case PHASE_ESTABLISH: return "establish";
		case PHASE_AUTHENTICATE: return "authenticate";
		case PHASE_CALLBACK: return "callback";
		case PHASE_NETWORK: return "network";
		case PHASE_RUNNING: return "running";
		case PHASE_TERMINATE: return "terminate";
		case PHASE_DISCONNECT: return "disconnect";
		case PHASE_HOLDOFF: return "holdoff";
	}
	p = p->next;
	sprintf(p->buf,"unknown-%d", phase);
	return p->buf;
}

/* -------------------------------------------------------------------- */

int capi_new_phase(int phase)
{
	if (phase == curphase) {
	   info("phase %s, again.", phase2str(phase));
	   return 0;
	}
        if (curphase != -1) {
           info("phase %s (was %s).",
		phase2str(phase), phase2str(curphase));
	} else {
	   info("phase %s.", phase2str(phase));
	}
	curphase = phase;
	switch (phase) {
		case PHASE_INITIALIZE:
		case PHASE_ESTABLISH:
		case PHASE_AUTHENTICATE:
		case PHASE_CALLBACK:
		case PHASE_NETWORK:
		case PHASE_RUNNING:
		case PHASE_TERMINATE:
		case PHASE_DISCONNECT:
		case PHASE_HOLDOFF:
			break;

		case PHASE_DEAD:
                        disconnectall();
			break;

		case PHASE_DORMANT:
			status = EXIT_OK;
			itun_check_options();
			init_capiconn();
			if (opt_inmsn || opt_cli) {
			   if (wakeupnow)
                              wakeupdemand();
		           setupincoming_for_demand();
			}
			break;

		case PHASE_SERIALCONN:
			status = EXIT_OK;
		        wakeupnow = 0;
	                if (conn_isconnected(0))
			   break;
			itun_check_options();
			init_capiconn();
			if (opt_number) {
			     if (coso == COSO_REMOTE) {
				     makeconnection_with_callback();
			     } else {
				     makeconnection(numbers);
			     }
			} else if (opt_channels) {
			     makeleasedline();
			} else {
			     waitforcall();
			}
			break;
	}
	return 0;
}

/* -------------------------------------------------------------------- */
/* -------- init/exit function ---------------------------------------- */
/* -------------------------------------------------------------------- */

int itun_init(void)
{
	int serrno;
	int err;

	if ((err = capi20_register (1, 8, 2048, &applid)) != 0) {
		serrno = errno;
		fatal("CAPI_REGISTER failed - %s (0x%04x) [%s (%d)]",
			capi_info2str(err), err,
			strerror(serrno), errno);
		return(-1);
        }
	
	if ((ctx = capiconn_getcontext(applid, &callbacks)) == 0) {
		(void)capi20_release(applid);
		fatal("get_context failed");
		return(-1);
	}
	info("init");
	return(capi20_fileno(applid));
}

void itun_exit(void)
{
	exiting = 1;
	disconnectall();
	(void)capi20_release(applid);
	info("exit");
}

