/*
 * Data handling functions
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

#ifndef __ITUN_DATA_H__
#define __ITUN_DATA_H__

#include "capiconn.h"

extern int data_sent;
extern time_t idle_last_use;

extern void handle_if_data(void);
extern void empty_interface(void);
extern void handle_capi_data(capi_connection *con, unsigned char* data, unsigned int len);
extern void handle_capi_sent(capi_connection *con, unsigned char* data);
extern void capi_is_connected(capi_connection *con);
extern void capi_is_disconnected(capi_connection *con);
extern int is_connected(void);

#endif /* __ITUN_DATA_H__ */
