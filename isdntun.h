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

#ifndef __ISDNTUN_H__
#define __ISDNTUN_H__

#include "debug.h"
#include "data.h"

extern void die(int val);
extern int status;
extern int iphase;

extern int itun_init(void);
extern void itun_exit(void);
extern int capi_new_phase(int phase);
extern void capi_wake_up(void);

extern void new_phase(int phase);

extern int if_fd;
extern int capi_fd;

extern void handle_capi_message(void);

/*
 * Values for phase.
 */
#define PHASE_DEAD              0
#define PHASE_INITIALIZE        1
#define PHASE_SERIALCONN        2
#define PHASE_DORMANT           3
#define PHASE_ESTABLISH         4
#define PHASE_AUTHENTICATE      5
#define PHASE_CALLBACK          6
#define PHASE_NETWORK           7
#define PHASE_RUNNING           8
#define PHASE_TERMINATE         9
#define PHASE_DISCONNECT        10
#define PHASE_HOLDOFF           11

/*
 * Exit status values.
 */
#define EXIT_OK                 0
#define EXIT_FATAL_ERROR        1
#define EXIT_OPTION_ERROR       2
#define EXIT_NOT_ROOT           3
#define EXIT_NO_KERNEL_SUPPORT  4
#define EXIT_USER_REQUEST       5
#define EXIT_LOCK_FAILED        6
#define EXIT_OPEN_FAILED        7
#define EXIT_CONNECT_FAILED     8
#define EXIT_PTYCMD_FAILED      9
#define EXIT_NEGOTIATION_FAILED 10
#define EXIT_PEER_AUTH_FAILED   11
#define EXIT_IDLE_TIMEOUT       12
#define EXIT_CONNECT_TIME       13
#define EXIT_CALLBACK           14
#define EXIT_PEER_DEAD          15
#define EXIT_HANGUP             16
#define EXIT_LOOPBACK           17
#define EXIT_INIT_FAILED        18
#define EXIT_AUTH_TOPEER_FAILED 19
#define EXIT_TRAFFIC_LIMIT      20
#define EXIT_CNID_AUTH_FAILED   21


#endif /* __ISDNTUN_H__ */

