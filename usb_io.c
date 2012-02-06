
/* $Id: usb_io.c,v 1.2 2005-12-16 02:05:19 steveb Exp $ */

/*

  Copyright (C) 2004 Peter Urbanec <toppy at urbanec.net>

  This file is part of puppy.

  puppy is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  puppy is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with puppy; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#include <assert.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <asm/byteorder.h>
#include "usb_io.h"

/* Linux usbdevfs has a limit of one page size per read/write.
   4096 is the most portable maximum we can do for now.
*/

/* REVISIT: perhaps dynamically determine if this needs to be 4096 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define MAX_READ    16384
#define MAX_WRITE   16384
#else
#define MAX_READ    4096
#define MAX_WRITE   4096
#endif

static inline unsigned short get_u16(const void *addr)
{
    const unsigned char *b = addr;

    return ((b[0] << 8) & 0xff00) | ((b[1] << 0) & 0x00ff);
}

/* This function is adapted from libusb */
ssize_t usb_bulk_write(struct usb_dev_handle *dev, int ep, const __u8 * bytes, ssize_t length, int timeout)
{
    struct usbdevfs_bulktransfer bulk;
    ssize_t ret;
    ssize_t sent = 0;

    /* Ensure the endpoint direction is correct */
    ep &= ~USB_ENDPOINT_DIR_MASK;

    do
    {
        bulk.ep = ep;
        bulk.len = length - sent;
        if(bulk.len > MAX_WRITE)
        {
            bulk.len = MAX_WRITE;
        }

        bulk.timeout = timeout;
        bulk.data = (__u8 *) bytes + sent;

        ret = ioctl(dev->fd, USBDEVFS_BULK, &bulk);
        if(ret < 0)
        {
#ifdef DEBUG
            fprintf(stderr, "error writing to bulk endpoint 0x%x: %s\n",
                    ep, strerror(errno));
#endif
        }
        else
        {
            sent += ret;
        }
    }
    while((ret > 0) && (sent < length));

    if((length % 0x200) == 0)
    {
        fprintf(stderr,
                "WARNING: USB I/O is modulo 0x200 - this can trigger a bug in Topfield firmware.\n");
    }

    return sent;
}

/* This function is adapted from libusb */
ssize_t usb_bulk_read(struct usb_dev_handle *dev, int ep, __u8 * bytes, ssize_t size, int timeout)
{
    struct usbdevfs_bulktransfer bulk;
    ssize_t ret;
    ssize_t retrieved = 0;
    ssize_t requested;

    memset(bytes, 0, size);

    /* Ensure the endpoint address is correct */
    ep |= USB_ENDPOINT_DIR_MASK;

    do
    {
        bulk.ep = ep;
        requested = size - retrieved;
        if (requested > MAX_READ) {
            requested = MAX_READ;
        }
        bulk.len = requested;
        bulk.timeout = timeout;
        bulk.data = (unsigned char *) bytes + retrieved;

        ret = ioctl(dev->fd, USBDEVFS_BULK, &bulk);
        if(ret < 0)
        {
#ifdef DEBUG
            fprintf(stderr, "error %d reading from bulk endpoint 0x%x: %s\n",
                    errno, ep, strerror(errno));
#endif
        }
        else
        {
            retrieved += ret;
        }
    }
    while((ret > 0) && (retrieved < size) && (ret == requested));

    return retrieved;
}

