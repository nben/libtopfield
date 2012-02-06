/* $Id: usbutil.c,v 1.4 2006/01/05 04:25:10 msteveb Exp $ */

/*

  Copyright (c) 2005 Steve Bennett <msteveb at ozemail.com.au>

  This file is part of libtopfield.

  libtopfield is free software; you can redistribute it and/or modify
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

#ifdef __linux
#include <linux/version.h>
#endif

#include <stdio.h>

#include "usbutil.h"

struct usb_dev_handle *open_usb_dev(int vendor_id, int product_id, int index)
{
	int err = 0;
	int found = 0;
	int count = 0;
	struct usb_dev_handle *devh = NULL;
	struct usb_device *usb_dev = NULL;
	struct usb_bus *bus = NULL;

	usb_init();
	usb_find_busses();
	usb_find_devices();

	/* Loop through busses and devices to find the nth device with
	 * a matching vendor and product id
	 */
	for (bus = usb_busses; bus; bus = bus->next) {
		for (usb_dev = bus->devices; usb_dev; usb_dev = usb_dev->next) {
			if ((usb_dev->descriptor.idVendor == vendor_id) &&
				(usb_dev->descriptor.idProduct == product_id)) {
				if (count++ == index) {
					found = 1;
					break;
				}
			}
		}
		if (found) {
			break;
		}
	}

	if (!found) {
		return 0;
	}

	devh = usb_open(usb_dev);

	if (!devh) {
		fprintf(stderr, "usb_open returned failure\n");
		return 0;
	}

	if((err = usb_claim_interface(devh, 0))) {
		fprintf(stderr, "usb_claim_interface returned an error (%d)\n", err);
		usb_close(devh);
		return 0;
	}

#if 0
	/* It's not clear that these are needed */
	if((err = usb_set_configuration(devh, 1))) {
		fprintf(stderr, "usb_set_configuration returned an error (%d)\n", err);
		close_usb_dev(devh);
		return 0;
	}

	if((err = usb_set_altinterface(devh, 0))) {
		fprintf(stderr, "usb_set_altinterface returned an error (%d)\n", err);
		close_usb_dev(devh);
		return 0;
	}
#endif
	return devh;
}

void close_usb_dev(struct usb_dev_handle *devh)
{
#if defined(__linux)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) && LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,10)
#define DO_USB_RESET
#endif
#endif

#ifdef DO_USB_RESET
	/* On Linux 2.6 before 2.6.11, using usb_reset() here significantly speeds
	 * up reconnection.
	 */
	usb_reset(devh);
#else
	if (usb_release_interface(devh, 0)) {
		fprintf(stderr, "usb_release_interface returned an error!\n");
	}

	usb_close(devh);
#endif
}
