
/* $Id: tf_bytes.h,v 1.2 2006/01/05 04:25:10 msteveb Exp $ */

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

#ifndef _TF_BYTES_H
#define _TF_BYTES_H 1

/* The Topfield packet handling is a bit unusual. All data is stored in
 * memory in big endian order, however, just prior to transmission all
 * data is byte swapped.
 *
 * We try to handle this in here in a some kind of a portable manner.
 *
 * We provide functions to read and write the memory version of packets
 * under the name get_X() and put_X().
 *
 * The USB I/O layer then takes care of CRC generation and byte swapping.
 */

#include "tf_types.h"

#include <stdlib.h>

/* The following functions retrieve a 16 bit, 32 bit or 64 bit
 * value in little-endian format from the given buffer address.
 */
__u16 get_u16(const void *addr);
__u32 get_u24(const void *addr);
__u32 get_u32(const void *addr);
__u64 get_u64(const void *addr);

/* The following functions retrieve a 16 bit or 32 bit value
 * from a byte swapped buffer at the given address
 */
__u32 get_u32_raw(const void *addr);
__u16 get_u16_raw(const void *addr);

/* The following functions store a 16 bit, 32 bit or 64 bit
 * value in little-endian format from the given buffer address.
 */
void put_u16(void *addr, __u16 val);
void put_u32(void *addr, __u32 val);
void put_u64(void *addr, __u64 val);

/**
 * Byte swap the given number of bytes (must be even)
 * in the given buffer.
 * This means swap bytes 0 and 1, butes 2 and 3, etc.
 */
void byte_swap(void *packet, size_t count);

#endif /* _TF_BYTES_H */
