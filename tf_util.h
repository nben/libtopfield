/* $Id: tf_util.h,v 1.1.1.1 2005/11/23 10:25:07 steveb Exp $ */

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
#ifndef TF_UTIL_H
#define TF_UTIL_H

/* Various useful functions for working with tf_io */

#include "tf_io.h"

/**
 * Examines the given pathname and fills in '*dirent' with the details.
 * Returns 0 if OK or 1 if the file or directory is not found or < 0 on error.
 */
int tf_stat(tf_handle *tf, const char *path, tf_dirent *dirent);

/**
 * Takes a directory and a filename and produces an absolute pathname
 * which refers to the file.
 * 
 * The directory is always relative to the root directory.
 * Either / or "" refers to the root directory.
 * Either /abc or "abc" refers to /abc.
 * 
 * The filename may be NULL to simply canonicalise the directory name.
 * If the filename begins with /, the directory is completely ignored.
 * This makes it easy for this provide chdir() behaviour.
 * 
 * The result is stored in a static buffer.
 */
char *tf_makename(const char *dir, const char *filename);

enum {
	TF_SORT_NAME = 1,	/* type then name */
	TF_SORT_SIZE,		/* type then size */
	TF_SORT_TIME,		/* type then time */
};

/**
 * Sorts the directory entries returned from tf_cmd_dir_first()/tf_cmd_dir_next()
 * according to the given sort type.
 *
 * Use a negative number (e.g. -TF_SORT_NAME) to sort in reverse order.
 */
void tf_sort_dirents(tf_dir_entries *entries, int sort_type);

#endif
