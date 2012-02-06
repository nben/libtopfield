
/* $Id: usb_io.h,v 1.2 2005-12-16 02:05:19 steveb Exp $ */

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

#ifndef _USB_IO_H
#define _USB_IO_H 1

#ifdef USE_LIBUSB
#include <usb.h>
#else

#include <sys/types.h>
#include <linux/types.h>
#include <linux/version.h>

/* linux/usb_ch9.h wasn't separated out until 2.4.23 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,23)
#include <linux/usb_ch9.h>
#else
#include <linux/usb.h>
#endif
#include <linux/usbdevice_fs.h>

struct usb_dev_handle {
	int fd;
};

typedef struct usb_dev_handle usb_dev_handle;

ssize_t usb_bulk_write(struct usb_dev_handle *dev, int ep, const __u8 * bytes, ssize_t length, int timeout);
ssize_t usb_bulk_read(struct usb_dev_handle *dev, int ep, __u8 * bytes, ssize_t size, int timeout);

#endif

#endif /* _USB_IO_H */
