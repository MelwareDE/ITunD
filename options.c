/*
 * Command line option handling
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

#include <strings.h>

#include "isdntun.h"
#include "options.h"

int daemonize = 1;
char *opt_encap = "ether";
char *opt_controller;
char *opt_numberprefix = 0;
char *opt_number = 0;
char *opt_callbacknumber = 0;
char *opt_msn = 0;
char *opt_inmsn = 0;
char *opt_cli = 0;
char *opt_proto = "hdlc";
char *opt_channels = 0;
char *opt_coso = 0;
char *opt_ddi = 0;
int opt_dialtimeout = 10;
int opt_dialmax = 3;
int opt_redialdelay = 5;
int opt_cbdelay = 2;
int opt_cbflag  = 0;
int opt_cbwait = 20;
int opt_connectdelay = 0;
int opt_acceptdelayflag  = 0;
int opt_voicecallwakeup  = 0;

int demand = 0;
int opt_idle = 300;
int opt_persist = 1;
int encap = ENCAP_ETHER;
int ether_flag = 1;
char *opt_name = NULL;
char *opt_ifup = NULL;

/*
 * show options
 */
void debug_options(void)
{
	dbglog("daemonize       = %4d", daemonize);
	dbglog("debug_level     = %4d", debug_level);
	dbglog("name            = %s", opt_name);
	dbglog("idle            = %4d", opt_idle);
	dbglog("encap           = %s", opt_encap);
	dbglog("controller      = %s", opt_controller);
	dbglog("ddi             = %s", opt_ddi);
	dbglog("numberprefix    = %s", opt_numberprefix);
	dbglog("number          = %s", opt_number);
	dbglog("callbacknumber  = %s", opt_callbacknumber);
	dbglog("msn             = %s", opt_msn);
	dbglog("inmsn           = %s", opt_inmsn);
	dbglog("cli             = %s", opt_cli);
	dbglog("proto           = %s", opt_proto);
	dbglog("channels        = %s", opt_channels);
	dbglog("coso            = %s", opt_coso);
	dbglog("dialtimeout     = %4d", opt_dialtimeout);
	dbglog("dialmax         = %4d", opt_dialmax);
	dbglog("redialdelay     = %4d", opt_redialdelay);
	dbglog("cbdelay         = %4d", opt_cbdelay);
	dbglog("clicb           = %4d", opt_cbflag);
	dbglog("cbwait          = %4d", opt_cbwait);
	dbglog("connectdelay    = %4d", opt_connectdelay);
	dbglog("acceptdelay     = %4d", opt_acceptdelayflag);
	dbglog("voicecallwakeup = %4d", opt_voicecallwakeup);
	dbglog("demand          = %4d", demand);
	dbglog("persist         = %4d", opt_persist);
}

int check_global_options(void)
{
	/*
	 * encapsulation 
	 */
	if (strcasecmp(opt_encap, "ether") == 0) {
	   encap = ENCAP_ETHER;
	   ether_flag = 1;
	} else if (strcasecmp(opt_encap, "rawip") == 0) {
	   encap = ENCAP_RAWIP;
	   ether_flag = 0;
	} else if (strcasecmp(opt_encap, "zipip") == 0) {
	   encap = ENCAP_ZIPIP;
	   ether_flag = 0;
	} else {
	   option_error("unknown encap \"%s\"", opt_encap);
	   die(1);
	}

	return(1);
}

