/* $Id: usbutil.h,v 1.1.1.1 2005/11/23 10:25:07 steveb Exp $ */

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
#ifndef USBUTIL_H
#define USBUTIL_H

#include "usb_io.h"

/**
 * Finds and opens a USB device by vendor/product id.
 * Set index to 0 to find the first device, 1 for the second device, etc.
 *
 * Returns the opened device handle if OK, or 0 if error.
 */
struct usb_dev_handle *open_usb_dev(int vendor_id, int product_id, int index);

/**
 * Close a previously opened usb device.
 */
void close_usb_dev(struct usb_dev_handle *devh);

#endif
