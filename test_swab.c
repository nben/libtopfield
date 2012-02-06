
/* $Id: test_swab.c,v 1.1.1.1 2005/11/23 10:25:07 steveb Exp $ */

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

#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>


/**
 * Tests the swab function.
 * 
 * The man page for swab() doesn't specify whether
 * it can swap a buffer in place, so test that here
 * Also test that swapping an odd length leaves the last byte unchanged
 */
int main(void)
{
	char buf[] = "01234567";
	char buf2[] = "0123456";

	swab(buf, buf, strlen(buf));

	printf("test_swab: After swapping, buf=%s\n", buf);

	assert(strcmp(buf, "10325476") == 0);

	swab(buf2, buf2, strlen(buf2));

	printf("test_swab: After swapping, buf2=%s\n", buf2);

	assert(strcmp(buf2, "1032546") == 0);

	return 0;
}
