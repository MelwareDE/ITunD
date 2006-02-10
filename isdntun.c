/*
 * ISDN tunnel Daemon "ItunD"
 *
 * Copyright 2004-2006 SYSGO Real-Time Solutions AG
 * Klein-Winternheim, Germany
 * All rights reserved.
 *
 * Author: Armin Schindler <armin.schindler@sysgo.com>
 *
 * Copyright 2004-2006 Cytronics & Melware
 *
 * Armin Schindler <armin@melware.de>
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <getopt.h>
#include <syslog.h>
#include <sys/select.h>

#include "isdntun.h"
#include "options.h"
#include "signals.h"
#include "tun.h"

static char pid_file[128];

int if_fd;
int capi_fd;

int status = EXIT_OK;
int iphase = PHASE_DEAD;

void new_phase(int phase)
{
	iphase = phase;
	capi_new_phase(phase);
}

/*
 * exit
 */
void die(int val)
{
	error("die with %d", val);
	itun_close(if_fd);
	itun_exit();
	unlink(pid_file);
	exit(val);
}

/*
 * usage
 */
static void usage(char *prg)
{
	fprintf(stderr, "ISDN-Tunnel Daemon Version %s (c) 2004-2006 Cytronics & Melware, SYSGO AG\n", VERSION);
	fprintf(stderr, "usage: %s <option> <option> ...\n", prg);
	fprintf(stderr, "options:\n");
	fprintf(stderr, "   -D                   Don't daemonize into background\n");
	fprintf(stderr, "   -d <level>           Set debug level (%d)\n", debug_level);
	fprintf(stderr, "   --encap=<arg>        Encapsulation (ether, rawip, zipip)\n");
	fprintf(stderr, "   --controller=<arg>   CAPI controller specification\n");
	fprintf(stderr, "   --ddi=<arg,len>      DDI-number,DDI-length\n");
	fprintf(stderr, "   --protcol=<arg>      Protocol (x75, hdlc)\n");
	fprintf(stderr, "   --number=<arg>       Number to call\n");
	fprintf(stderr, "                        (may be comma separated list)\n");
	fprintf(stderr, "   --numberprefix=<arg> Prefix for number\n");
	fprintf(stderr, "   --msn=<arg>          Number to call from\n");
	fprintf(stderr, "   --inmsn=<arg>        Called number for incoming calls\n");
	fprintf(stderr, "   --cli=<arg>          Calling number for incoming calls\n");
	fprintf(stderr, "                        (may be comma separated list)\n");
	fprintf(stderr, "   --clicb              Call number and wait for callback\n");
	fprintf(stderr, "   --cbwait=<arg>       Number of seconds to wait for callback\n");
	fprintf(stderr, "   --dialtimeout=<arg>  Number of seconds to wait for connection\n");
	fprintf(stderr, "                        or reject\n");
	fprintf(stderr, "   --dialmax=<arg>      Number of dial retries\n");
	fprintf(stderr, "   --redialdelay=<arg>  Number of seconds to wait between dial retries\n");
	fprintf(stderr, "   --channels=<arg>     Channel to use for leased line\n");
	fprintf(stderr, "                        (may be comma separated list)\n");
	fprintf(stderr, "   --cbdelay=<arg>      Number of seconds to wait before calling back\n");
	fprintf(stderr, "   --cbnumber=<arg>     Number to call (may be comma separated list)\n");
	fprintf(stderr, "   --connectdelay=<arg> Number of seconds to wait after connection is established\n");
	fprintf(stderr, "   --acceptdelay        Wait 1 second before accept incoming call\n");
	fprintf(stderr, "   --coso=<arg>         COSO: caller,local or remote\n");
	fprintf(stderr, "   --voicecallwakeup    Call number and wait for callback\n");
	fprintf(stderr, "   --demand             Establish connection on demand only\n");
	fprintf(stderr, "   --nopersist          Exit after connection\n");
	fprintf(stderr, "   --idle=<arg>         Number of idle seconds before hangup (%d)\n", opt_idle);
	fprintf(stderr, "   --name=<arg>         Personal name for this connection\n");
	fprintf(stderr, "   --ifup=<arg>         Script to be called when interface is up\n");
	fprintf(stderr, "                        called with arguments <if-name> <given-name>\n");
	fprintf(stderr, "\n");
}

/*
 * long options list
 */
static struct option itun_options[] = {
	{ "help",		no_argument,		NULL,	0x100 },
	{ "encap",		required_argument,	NULL,	0x101 },
	{ "controller",		required_argument,	NULL,	0x102 },
	{ "number",		required_argument,	NULL,	0x103 },
	{ "numberprefix",	required_argument,	NULL,	0x104 },
	{ "msn",		required_argument,	NULL,	0x105 },
	{ "protocol",		required_argument,	NULL,	0x106 },
	{ "inmsn",		required_argument,	NULL,	0x107 },
	{ "cli",		required_argument,	NULL,	0x108 },
	{ "clicb",		no_argument,		NULL,	0x109 },
	{ "cbwait",		required_argument,	NULL,	0x10A },
	{ "dialtimeout",	required_argument,	NULL,	0x10B },
	{ "dialmax",		required_argument,	NULL,	0x10C },
	{ "redialdelay",	required_argument,	NULL,	0x10D },
	{ "channels",		required_argument,	NULL,	0x10E },
	{ "cbdelay",		required_argument,	NULL,	0x10F },
	{ "cbnumber",		required_argument,	NULL,	0x110 },
	{ "connectdelay",	required_argument,	NULL,	0x111 },
	{ "acceptdelay",	no_argument,		NULL,	0x112 },
	{ "coso",		required_argument,	NULL,	0x113 },
	{ "voicecallwakup",	no_argument,		NULL,	0x114 },
	{ "demand",		no_argument,		NULL,	0x115 },
	{ "idle",		required_argument,	NULL,	0x116 },
	{ "nopersist",		no_argument,		NULL,	0x117 },
	{ "name",		required_argument,	NULL,	0x118 },
	{ "ddi",		required_argument,	NULL,	0x119 },
	{ "ifup",		required_argument,	NULL,	0x11A },
	{ NULL, 0, NULL, 0 }
};

/*
 * capi wants wake up
 */
void capi_wake_up(void)
{
	Dialsignal = 1;
	dbglog("capi_wake_up");
}

/*
 * main loop of connection handling
 */
static void connection_handling(void)
{
	fd_set rfds;
	struct timeval tv;
	int ret;
	int maxfd;

	FD_ZERO(&rfds);
	FD_SET(capi_fd, &rfds);
	maxfd = capi_fd;

	if (((iphase == PHASE_RUNNING) && (data_sent)) ||
	     (iphase == PHASE_DORMANT)) {
		FD_SET(if_fd, &rfds);
		if (if_fd > capi_fd)
			maxfd = if_fd;
	}

	tv.tv_sec = 1;
	tv.tv_usec = 0;

	ret = select(maxfd + 1, &rfds, NULL, NULL, &tv);

	if (ret > 0) {
		if (FD_ISSET(capi_fd, &rfds)) {
			dbglog("capi_fd in select");
			handle_capi_message();
		}

		if (FD_ISSET(if_fd, &rfds)) {
			dbglog("if_fd in select");
			if (is_connected())
				handle_if_data();
			else
				Dialsignal = 1;
		}
	}
}

/*
 * MAIN
 */
int main(int argc, char *argv[])
{
	int c;
	FILE *f;

	while ((c = getopt_long(argc, argv, "?hDd:", itun_options, (int *)0)) != EOF) {
		switch(c) {
			case 'D': /* don't daemonize */
				daemonize = 0;
				break;
			case 'd': /* debug level */
				debug_level = strtol(optarg, NULL, 0);
				break;
			case 0x101: /* encap */
				opt_encap = strdup(optarg);
				break;
			case 0x102: /* controller */
				opt_controller = strdup(optarg);
				break;
			case 0x103: /* number */
				opt_number = strdup(optarg);
				break;
			case 0x104: /* numberprefix */
				opt_numberprefix = strdup(optarg);
				break;
			case 0x105: /* msn */
				opt_msn = strdup(optarg);
				break;
			case 0x106: /* protocol */
				opt_proto = strdup(optarg);
				break;
			case 0x107: /* inmsn */
				opt_inmsn = strdup(optarg);
				break;
			case 0x108: /* cli */
				opt_cli = strdup(optarg);
				break;
			case 0x109: /* clicb */
				opt_cbflag = 1;
				break;
			case 0x10A: /* cbwait */
				opt_cbwait = strtol(optarg, NULL, 0);
				break;
			case 0x10B: /* dialtimeout */
				opt_dialtimeout = strtol(optarg, NULL, 0);
				break;
			case 0x10C: /* dialmax */
				opt_dialmax = strtol(optarg, NULL, 0);
				break;
			case 0x10D: /* redialdelay */
				opt_redialdelay = strtol(optarg, NULL, 0);
				break;
			case 0x10E: /* channels */
				opt_channels = strdup(optarg);
				break;
			case 0x10F: /* cbdelay */
				opt_cbdelay = strtol(optarg, NULL, 0);
				break;
			case 0x110: /* cbnumber */
				opt_callbacknumber = strdup(optarg);
				break;
			case 0x111: /* connectdelay */
				opt_connectdelay = strtol(optarg, NULL, 0);
				break;
			case 0x112: /* acceptdelay */
				opt_acceptdelayflag = 1;
				break;
			case 0x113: /* coso */
				opt_coso = strdup(optarg);
				break;
			case 0x114: /* voicecallwakeup */
				opt_voicecallwakeup = 1;
				break;
			case 0x115: /* demand */
				demand = 1;
				break;
			case 0x116: /* idle */
				opt_idle = strtol(optarg, NULL, 0);
				break;
			case 0x117: /* nopersist */
				opt_persist = 0;
				break;
			case 0x118: /* name */
				opt_name = strdup(optarg);
				break;
			case 0x119: /* ddi */
				opt_ddi = strdup(optarg);
				break;
			case 0x11A: /* ifup */
				opt_ifup = strdup(optarg);
				break;
			default:
				fprintf(stderr, "Unknown option %c\n", c);
				/* fall through */
			case 0x100: /* help */
			case 'h':
			case '?':
				usage(argv[0]);
				return(-1);
		}
	}

	/* open syslog */
	openlog("ItunD", LOG_PID, LOG_DAEMON);

	if (!check_global_options()) {
		usage(argv[0]);
		return(-1);
	}

	debug_options();

	signal_setup();
	
	if ((if_fd = itun_alloc(dev_name, ether_flag)) < 0) {
		perror("open alloc tun device");
		return(-1);
	}

	if (!opt_name)
		opt_name = strdup(dev_name);

	if ((capi_fd = itun_init()) < 0) {
		perror("init capi");
		itun_close(if_fd);
		return(-1);
	}

	if (daemonize) {
		/* make us a daemon in the background */
		switch(fork()) {
			case 0: /* fork ok - we are the child */
				close(0);
				close(1);
				dbglog("forked with pid %d", getpid());
				syslog(LOG_INFO, "%s: started", opt_name);
				break;
			case -1: /* fork error */
				perror("fork");
				exit(errno);
				break;
			default: /* fork ok - parent ends here */
				dbglog("child forked, parent ends.");
				printf("%s\n", dev_name);
				return(0);
		}
	} else {
		syslog(LOG_INFO, "%s: started", opt_name);
		printf("%s\n", dev_name);
	}

	/* create pid file */
	snprintf(pid_file, sizeof(pid_file)-1, "/var/run/%s.pid", dev_name);
	if ((f = fopen(pid_file, "w+")) != NULL) {
		fprintf(f, "%d\n", getpid());
		fclose(f);
	}

	new_phase(PHASE_DORMANT);

	/* run script to "interface-up" */
	if (opt_ifup) {
		char ifup_script[256];
		sprintf(ifup_script, "%s %s %s",
			opt_ifup, dev_name, opt_name);
		system(ifup_script);
	}

	/* if demand is not enabled, force dial */
	if (!demand) {
		Dialsignal = 1;
	}

	/* main loop */
	while (!Terminate) {

		if (Hupsignal) {
			Hupsignal = 0;
			if (iphase == PHASE_RUNNING) {
				new_phase(PHASE_DEAD);
			}
		}

		if (Dialsignal) {
			Dialsignal = 0;
			if (iphase == PHASE_DORMANT) {
				new_phase(PHASE_SERIALCONN);
				if (iphase != PHASE_RUNNING) {
					/* connection not established, so we clean up IF */
					empty_interface();
				}
			}
		}

		/* TODO: if the netif is down, hangup/reject call */

		connection_handling();

		if (iphase == PHASE_DISCONNECT) {
			if (opt_persist) {
				new_phase(PHASE_DORMANT);
			} else {
				Terminate = 1;
			}
		}
		
		/* check idle time */
		if ((iphase == PHASE_RUNNING) &&
		    (opt_idle) &&
		    ((opt_idle + idle_last_use) < time(NULL))) {
			info("idle for %d seconds, hanging up", opt_idle);
			Hupsignal = 1;
		}
	}

	new_phase(PHASE_TERMINATE);
	itun_close(if_fd);
	itun_exit();

	unlink(pid_file);

	closelog();

	return(0);
}

