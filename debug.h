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

#ifndef __ITUN_DEBUG_H__
#define __ITUN_DEBUG_H__

/*
 * debug
 */

extern int debug_level;
extern void fatal(char *format, ...);
extern void error(char *format, ...);
extern void info(char *format, ...);
extern void dbglog(char *format, ...);
extern void option_error(char *format, ...);


#endif /* __ITUN_DEBUG_H__ */

