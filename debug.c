/*
 * Debug handling
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
#include <stdarg.h>
#include <unistd.h>
#include <syslog.h>

#include "isdntun.h"
#include "options.h"

int debug_level = 2;

/*
 * syslog
 */
static void do_syslog(int prio, char *format, va_list ap)
{
	unsigned char buf[2048];
	int len;

	len = sprintf(buf, "%s: ", opt_name);
	vsnprintf(buf + len, sizeof(buf), format, ap);
	syslog(prio, buf);
}

/*
 * debug
 */
static void debug_output(int level, char *format, va_list ap)
{
	switch (level) {
		case 0:
		case 1:
		case 2:
			do_syslog(LOG_ERR, format, ap);
			break;
		case 3:
			do_syslog(LOG_INFO, format, ap);
			break;
	}
	
	if (level > debug_level)
		return;

	fprintf(stderr, "Itun %d: ", level);
	vfprintf(stderr, format, ap);
	fprintf(stderr, "\n");
}

void option_error(char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	debug_output(0, format, ap);
	va_end(ap);
}

void fatal(char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	debug_output(1, format, ap);
	va_end(ap);
	exit(-1);
}

void error(char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	debug_output(2, format, ap);
	va_end(ap);
}

void info(char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	debug_output(3, format, ap);
	va_end(ap);
}

void dbglog(char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	debug_output(4, format, ap);
	va_end(ap);
}

