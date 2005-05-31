/*
 * Signal handling functions
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <isdntun.h>

int Terminate = 0;
int Hupsignal = 0;
int Dialsignal = 0;

/*
 * SIGTERM signal handler
 */
static void sig_term(int sig)
{
	dbglog("Got signal TERM/INT");
	Terminate = 1;
	status = EXIT_USER_REQUEST;
}

/*
 * SIGHUP signal handler
 */
static void sig_hup(int sig)
{
	dbglog("Got signal HUP");
	Hupsignal = 1;
}

/*
 * SIGUSR1 signal handler
 */
static void sig_usr1(int sig)
{
	dbglog("Got signal USR1");
	Dialsignal = 1;
}

/*
 * SIGUSR2 signal handler
 */
static void sig_usr2(int sig)
{
	dbglog("Got signal USR2");
}

/*
 * SIGCHLD
 */
static void sig_chld(int sig)
{
	dbglog("Got signal CHLD");
}

/*
 * setup the signal handlers
 */
void signal_setup(void)
{
	sigset_t mask;
	struct sigaction sa;
	sigemptyset(&mask);
	sigaddset(&mask,SIGHUP);
	sigaddset(&mask,SIGTERM);
	sigaddset(&mask,SIGINT);
	sigaddset(&mask,SIGUSR1);
	sigaddset(&mask,SIGUSR2);
	sigaddset(&mask,SIGCHLD);
																	 
#define SIGNAL(s, handler)  { \
			sa.sa_handler = handler; \
			if (sigaction(s, &sa , NULL) < 0) { \
				fatal("ERROR setting up the signal handlers"); \
				exit(-2); \
			} \
		}
		
	sa.sa_mask = mask;
	sa.sa_flags = 0;
	SIGNAL(SIGHUP, sig_hup);
	SIGNAL(SIGTERM, sig_term);
	SIGNAL(SIGINT, sig_term);
	SIGNAL(SIGUSR1, sig_usr1);
	SIGNAL(SIGUSR2, sig_usr2);
	SIGNAL(SIGCHLD, sig_chld);
} 


