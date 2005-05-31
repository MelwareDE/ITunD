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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <zlib.h>

#include "isdntun.h"
#include "options.h"
#include "tun.h"
#include "capiconn.h"

int data_sent = 0;
time_t idle_last_use = 0;

static capi_connection *conn = NULL;

#undef DEBUG_DATA

/*
 * flush the data in the interface
 */
void empty_interface(void)
{
	fd_set rfds;
	struct timeval tv;
	int ret;
	unsigned char buffer[2048];

	info("empty interface %s queue", dev_name);

	while (1) {
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		FD_ZERO(&rfds);
		FD_SET(if_fd, &rfds);

		ret = select(if_fd + 1, &rfds, NULL, NULL, &tv);
		if (ret < 1)
			break;
		read(if_fd, buffer, sizeof(buffer));
		dbglog("flushed one buffer");
	}
}

/*
 * compress data
 */
static void compress_data(unsigned char *buf, int *len)
{
	unsigned long l1, l2;
	unsigned char b[2048];
	
	if (*len < 16)
		return;

	l1 = (unsigned long)*len;
	memcpy(b, buf, *len);

	l2 = sizeof(b);

	buf[0] = 0;
	if ((compress(buf + 1, &l2, b, l1) != Z_OK) ||
	   ((l2 + 1) > *len)) {
		memcpy(buf + 1, b, *len);
		*len += 1;
		dbglog("if data not compressed len=%d", *len);
		return;
	}
	*len = (int)(l2 + 1);
	buf[0] = 1;
	dbglog("if data compressed %ld->%d", l1, *len);
}

/*
 * compress data
 */
static void uncompress_data(unsigned char *buf, unsigned int *len)
{
	unsigned long l1, l2;
	unsigned char b[2048];

	memcpy(b, buf, *len);
	*len -= 1;
			
	if (*b == 0) { /* not compressed */
		memcpy(buf, b + 1, *len);
		return;
	}

	l1 = (unsigned long)*len;
	l2 = sizeof(b);

	if (uncompress(buf, &l2, b + 1, l1) != Z_OK) {
		*len = 0;
		error("uncompress error");
		return;
	}
	*len = (unsigned int)l2; 
	dbglog("if data uncompressed %ld->%d", l1, *len);
}

/*
 * handle data coming from device
 */
void handle_if_data(void)
{		
	int len;
	unsigned char buffer[2048];
	if ((len = read(if_fd, buffer, sizeof(buffer))) < 1) {
		error("error %d reading %s", len, dev_name);
		return;
	}

	#ifdef DEBUG_DATA
	{
		int i;
		FILE *f;
		if ((f = fopen("/tmp/itun.txt", "a")) != NULL) {
			fprintf(f, "inet %03d: ", len);
			for (i = 0; i < len; i++)
				fprintf(f, "%02x ", buffer[i]);
			fprintf(f, "\n");
			fclose(f);
		}
	}
	#endif

	if (encap == ENCAP_ZIPIP)
		compress_data(buffer, &len);

	data_sent = 0;

	if (capiconn_send(conn, buffer, len) != CAPICONN_OK) {
		error("data not sent");
	}

	idle_last_use = time(NULL);
}

/*
 * handle data coming from capi (isdn)
 */
void handle_capi_data(capi_connection *con, unsigned char* data, unsigned int len)
{
	int dlen;
	
	dlen = len;
	dbglog("capi data to if of len %d", dlen);

	if (encap == ENCAP_ZIPIP)
		uncompress_data(data, &dlen);

	if (write(if_fd, data, dlen) != dlen)
		error("Error writing isdn data to %s", dev_name);

	idle_last_use = time(NULL);

	#ifdef DEBUG_DATA
	{
		int i;
		FILE *f;
		if ((f = fopen("/tmp/itun.txt", "a")) != NULL) {
			fprintf(f, "isdn %03d: ", dlen);
			for (i = 0; i < dlen; i++)
				fprintf(f, "%02x ", data[i]);
			fprintf(f, "\n");
			fclose(f);
		}
	}
	#endif
}

/*
 * isdn data have been sent
 */
void handle_capi_sent(capi_connection *con, unsigned char* data)
{
	dbglog("data sent confirmed");
	data_sent = 1;
}

/*
 * get status of connection
 */
int is_connected(void)
{
	if (conn)
		return(1);
	return(0);
}

/*
 * if connection is established, store connection pointer
 */
void capi_is_connected(capi_connection *con)
{
	conn = con;
	data_sent = 1; /* ready for data */
	idle_last_use = time(NULL);
	new_phase(PHASE_RUNNING);
}

/*
 * if connection is released, clear connection pointer
 */
void capi_is_disconnected(capi_connection *con)
{
	conn = NULL;
	new_phase(PHASE_DISCONNECT);
}

