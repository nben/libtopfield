/* $Id: usb_io_util.c,v 1.2 2005/11/29 13:35:57 steveb Exp $ */

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
#include <linux/version.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "usbutil.h"

#define MAX_DEVICES_LINE_SIZE 128

struct usb_dev_handle *open_usb_dev(int vendor_id, int product_id, int index)
{
	int fd;
    FILE *fh;
    char buffer[MAX_DEVICES_LINE_SIZE];
	int bus = 0;
	int device = 0;
	int found = 0;
	int count = 0;
	int interface = 0;
	struct usbdevfs_setinterface interface0 = { 0, 0 };
	struct usb_dev_handle *dev;

    /* Open the /proc/bus/usb/devices file, and read it to find candidate Topfield devices. */
    fh = fopen("/proc/bus/usb/devices", "r");
	if (!fh) {
		perror("open /proc/bus/usb/devices");
		return 0;
	}

    /* Scan the devices file, line by line, looking for Topfield devices. */
    while(!found && fgets(buffer, MAX_DEVICES_LINE_SIZE, fh) != 0) {
		unsigned int v, p;

        /* Store the information from topology lines. */
        if (sscanf(buffer, "T: Bus=%d Lev=%*d Prnt=%*d Port=%*d Cnt=%*d Dev#=%d", &bus, &device) == 2) {
			/* Identified bus/device */
			continue;
        }

		if (!bus || !device) {
			continue;
		}

        /* Look for Topfield vendor/product lines, and also check for multiple devices. */
        if (sscanf(buffer, "P: Vendor=%x ProdID=%x", &v, &p) == 2) {
			if (v == vendor_id && p == product_id) {
				if (count++ == index) {
					found = 1;
					break;
				}
			}
		}
    }
    fclose(fh);

	if (!found) {
		return 0;
	}

	/* Construct the device path according to the topology found. */
	snprintf(buffer, sizeof(buffer), "/proc/bus/usb/%03d/%03d", bus, device);

    fd = open(buffer, O_RDWR);
    if (fd < 0) {
		fprintf(stderr, "ERROR: Failed to open usb device %s: %m\n", buffer);
        return 0;
    }

    if (ioctl(fd, USBDEVFS_RESET, NULL) < 0) {
        fprintf(stderr, "ERROR: Can not reset device: %m\n");
		/* Hmmm. The NSLU2 with 2.4.22 seems to have trouble resetting sometimes.
		 * carrying on anyway seems to work OK
		 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
        close(fd);
        return 0;
#endif
    }

	if (ioctl(fd, USBDEVFS_CLAIMINTERFACE, &interface) < 0) {
		fprintf(stderr, "ERROR: Can not claim interface 0: %m\n");
		close(fd);
        return 0;
    }

	if (ioctl(fd, USBDEVFS_SETINTERFACE, &interface0) < 0) {
		fprintf(stderr, "ERROR: Can not set interface zero: %m\n");
		close(fd);
		return 0;
    }

	dev = malloc(sizeof(*dev));
	dev->fd = fd;

	return dev;
}

void close_usb_dev(struct usb_dev_handle *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) && LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,10)
	/* On Linux 2.6 before 2.6.11, using usb_reset() here significantly speeds
	 * up reconnection.
	 */
    ioctl(dev->fd, USBDEVFS_RESET, NULL);
#else
	int interface = 0;

	ioctl(dev->fd, USBDEVFS_RELEASEINTERFACE, &interface);
#endif
	close(dev->fd);
	free(dev);
}
