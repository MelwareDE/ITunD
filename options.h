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

#ifndef __ITUN_OPTIONS_H__
#define __ITUN_OPTIONS_H__


#define ENCAP_ETHER	0
#define ENCAP_RAWIP	1
#define ENCAP_ZIPIP	2


extern int daemonize;

extern char *opt_encap;
extern char *opt_controller;
extern char *opt_numberprefix;
extern char *opt_number;
extern char *opt_callbacknumber;
extern char *opt_msn;
extern char *opt_inmsn;
extern char *opt_cli;
extern char *opt_proto;
extern char *opt_channels;
extern char *opt_coso;
extern char *opt_ddi;
extern int opt_dialtimeout;
extern int opt_dialmax;
extern int opt_redialdelay;
extern int opt_cbdelay;
extern int opt_cbflag;
extern int opt_cbwait;
extern int opt_connectdelay;
extern int opt_acceptdelayflag;
extern int opt_voicecallwakeup;

extern int demand;
extern int opt_idle;
extern int opt_persist;
extern int encap;
extern int ether_flag;

extern char *opt_name;
extern char *opt_ifup;

extern void debug_options(void);
extern int check_global_options(void);

#endif /* __ITUN_OPTIONS_H__ */

