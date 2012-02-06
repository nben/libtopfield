
/* $Id: test_makename.c,v 1.1.1.1 2005/11/23 10:25:07 steveb Exp $ */

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "tf_util.h"

/**
 * Tests the tf_makename() function.
 */
int main(void)
{
	/* dir, file, expected */
	static const char *parts[] = {
		"", 0, "/",
		"", "", "/",
		"/", "", "/",
		"/", "", "/",
		"", "file", "/file",
		"/", "file", "/file",
		"/", "file", "/file",
		"/dir", "file", "/dir/file",
		"/dir", "/file", "/file",
		"/dir/", "file", "/dir/file",
		"/dir/", "/file", "/file",
		"/dir/", "/file/", "/file",
		"dir/", "/file/", "/file",
		"dir", "file", "/dir/file",
		"/dir/", "/file/", "/file",
		"/dir/../newdir", "file", "/newdir/file",
		"/../newdir", "file", "/newdir/file",
		"/dir/olddir", "..", "/dir",
		"/dir/olddir", "../newdir", "/dir/newdir",
	};
	static int num = sizeof(parts) / sizeof(*parts);
	int i;

	for (i = 0; i < num; i += 3) {
		const char *dir = parts[i];
		const char *file = parts[i + 1];
		const char *expected = parts[i + 2];
		char *buf = tf_makename(dir, file);

		printf("test_makename: dir='%s' filename='%s' => '%s'\n", dir, file, buf);

		assert(strcmp(buf, expected) == 0);
	}

	return 0;
}
