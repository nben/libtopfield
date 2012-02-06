/* $Id: tf_open.h,v 1.2 2005/11/23 10:54:43 steveb Exp $ */

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
#ifndef TF_OPEN_H
#define TF_OPEN_H

/* Opens a connection to a Topfield 5000PVR device */

#include <stdio.h>

struct usb_dev_handle;

/* Default number of milliseconds to wait for a packet transfer to complete.
 * This needs to be fairly long in case the disk needs to spin up from sleep mode
 */
#define TF_DEFAULT_TIMEOUT 11000

/* Note that this is the lockfile for device 0.
 * Device 1 use /tmp/puppy.1, etc.
 */
#define TF_LOCKFILE "/tmp/puppy"

/**
 * This handle represents a connection to a Topfield device.
 */
typedef struct {
	/* The following fields may be set after topfield_open() */
	int timeout;				/* Timeout for commands, in milliseconds */
	int trace_level;			/* 0 = none */
	FILE *tracefh;				/* Debug trace filehandle */
	int nocrc;					/* If set, crc is not checked on received packets */

	/* The following fields may be accessed */
	int error;					/* Last error, or 0 if no error */

	/* The following fields should not be touched */
	int lock_fd;			/* File used for locking to avoid Linux kernel bugs */
	struct usb_dev_handle *dev;	/* USB handle to the device */
	int pending;				/* A reply should be pending */
	char *buf;					/* Buffer used for transferring data during get/put */
} tf_handle;

typedef enum {
	TF_LOCK_STD,		/* Use this */
	TF_LOCK_NONE,		/* Do not use! */
	TF_LOCK_HUP			/* Send SIGHUP, wait a moment and try again */
} tf_lock_t;

/**
 * Opens a connection to the topfield device at the given index
 * (0 is the first, 1 is the second, etc.).
 * 
 * '*tf' is filled in if successful (and topfield_close() should
 * be called when done)
 *
 * Pass the appropriate locking type as 'lock'
 *
 * Returns 0 if OK, 1 if already locked/in-use or < 0 on error.
 */
int topfield_open(tf_handle *tf, int index, tf_lock_t lock);

/**
 * Closes a previously opened topfield device.
 */
void topfield_close(tf_handle *tf);

#endif
