/*
 * TUN/TAP driver interface functions 
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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/if.h>
#include <linux/if_tun.h>

unsigned char dev_name[IFNAMSIZ] = {"isdn%d"};

int itun_alloc(char *dev, int ether)
{
	struct ifreq ifr;
	int fd, err;

	if((fd = open("/dev/net/tun", O_RDWR)) >= 0) {
		memset(&ifr, 0, sizeof(ifr));

		/* Flags: IFF_TUN   - TUN device (no Ethernet headers, PTP)
		 *        IFF_TAP   - TAP device (with Ethernet headers)
		 *
		 *        IFF_NO_PI - Do not provide packet information
		 */
		ifr.ifr_flags = (ether) ? IFF_TAP : IFF_TUN;

		ifr.ifr_flags |= IFF_NO_PI;

		if (*dev)
			strncpy(ifr.ifr_name, dev, IFNAMSIZ);

		if((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0) {
			close(fd);
			return(err);
		}
		strcpy(dev, ifr.ifr_name);
	}
	return(fd);
}

void itun_close(int fd)
{
	close(fd);
}

