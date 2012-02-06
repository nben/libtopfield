/* $Id: tf_util.c,v 1.1.1.1 2005/11/23 10:25:07 steveb Exp $ */

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
#include <string.h>
#include <stdlib.h>

#include "tf_util.h"

int tf_stat(tf_handle *tf, const char *path, tf_dirent *dirent)
{
	char *pt;

	/* If this is the root directory, just fake the result */
	if (strcmp(path, "/") == 0) {
		dirent->type = 'd';
		dirent->stamp = 0;
		dirent->size = 0;
		dirent->attrib = 0;
		strcpy(dirent->name, "/");
		return 0;
	}
	else {
		int ret;
		int rc;
		tf_dir_entries entries;

		/* Examine the parent directory */
		char *parent;
		pt = strrchr(path, '/');
		if (!pt) {
			/* Not a valid file or directory */
			fprintf(stderr, "Directory '%s' has no parent\n", path);
			return 1;
		}
		if (pt == path) {
			/* In the root directory, like "\abc" */
			parent = strdup("/");
		}
		else {
			parent = strdup(path);
			parent[pt - path] = 0;
		}
		path = pt + 1;

		ret = tf_cmd_dir_first(tf, parent, &entries);

		rc = 1;
		if (ret != 0) {
			/* Failed to examine parent directory */
			fprintf(stderr, "parent dir_first failed for '%s'\n", parent);
		} else {
			while (ret == 0) {
				int i;

				for (i = 0; i < entries.count; i++) {
					/*fprintf(stderr, "Comparing %s with %s\n", entries.entry[i].name, path);*/
					if (strcmp(entries.entry[i].name, path) == 0) {
						*dirent = entries.entry[i];
						tf_cmd_dir_cancel(tf);
						rc = 0;
						break;
					}
				}
				if (rc == 0) {
					break;
				}
				ret = tf_cmd_dir_next(tf, &entries);
			}
		}

		free(parent);
		return rc;
	}
}

char *tf_makename(const char *dir, const char *filename)
{
	static char *buf;
	char *pt;

	if (buf) {
		free(buf);
	}

	/* Allow space for extra leading /, separator / and trailing null */
	buf = malloc(1 + strlen(dir) + 1 + strlen(filename ?: "") + 1);

	pt = buf;

	/* We always add a leading slash */
	*pt++ = '/';

	if (filename && *filename == '/') {
		dir = filename;
		filename = 0;
	}

	if (dir[0] == '/') {
		dir++;
	}

	/* Copy in 'dir' */
	while (*dir) {
		if (pt[-1] == '/' && dir[0] == '.' && dir[1] == '.') {
			if (dir[2] == '/' || dir[2] == 0) {
				/* Back up a directory */
				if (pt > buf + 1) {
					pt[-1] = 0;
					pt = strrchr(buf, '/') + 1;
				}
				else {
					pt = buf + 1;
				}

				dir += 2;
				if (*dir) {
					dir++;
				}
				continue;
			}
		}
		*pt++ = *dir++;
	}

	/* Now add the filename */
	if (filename) {
		/* Do we need a separator? */
		if (pt[-1] != '/') {
			*pt++ = '/';
		}

		if (filename[0] == '/') {
			filename++;
		}

		while (*filename) {
			if (pt[-1] == '/' && filename[0] == '.' && filename[1] == '.') {
				if (filename[2] == '/' || filename[2] == 0) {
					/* Back up a directory */
					if (pt > buf + 1) {
						pt[-1] = 0;
						pt = strrchr(buf, '/') + 1;
					}
					else {
						pt = buf + 1;
					}

					filename += 2;
					if (*filename) {
						filename++;
					}
					continue;
				}
			}
			*pt++ = *filename++;
		}
	}

	/* If we have a trailing slash, remove it, unless all we have is a slash */
	if (pt > buf + 1 && pt[-1] == '/') {
		pt--;
	}

	*pt = 0;

	/*fprintf(stderr, "tf_makename() returning %s\n", buf);*/

	return buf;
}

static int tf_cmp_dirent_by_type(const tf_dirent *de1, const tf_dirent *de2)
{
	/* Directories always come first */
	return de1->type - de2->type;
}

/**
 * Compare by type, then by timestamp.
 */
static int tf_cmp_dirent_by_time(const void *d1, const void *d2)
{
	const tf_dirent *de1 = (const tf_dirent *)d1;
	const tf_dirent *de2 = (const tf_dirent *)d2;

	int ret = tf_cmp_dirent_by_type(de1, de2);

	if (ret == 0) {
		ret = de2->stamp - de1->stamp;
	}
	return ret;
}

/**
 * Compare by type, then by size.
 */
static int tf_cmp_dirent_by_size(const void *d1, const void *d2)
{
	const tf_dirent *de1 = (const tf_dirent *)d1;
	const tf_dirent *de2 = (const tf_dirent *)d2;

	int ret = tf_cmp_dirent_by_type(de1, de2);

	if (ret == 0) {
		ret = (de1->size < de2->size) ? 1 : (de1->size == de2->size) ? 0 : -1;
	}
	return ret;
}

/**
 * Compare by type, then by (case insensitive) name.
 */
static int tf_cmp_dirent_by_name(const void *d1, const void *d2)
{
	const tf_dirent *de1 = (const tf_dirent *)d1;
	const tf_dirent *de2 = (const tf_dirent *)d2;

	int ret = tf_cmp_dirent_by_type(de1, de2);

	if (ret == 0) {
		ret = strcasecmp(de1->name, de2->name);
	}
	return ret;
}

static int tf_cmp_dirent_by_reverse_name(const void *d1, const void *d2)
{
	const tf_dirent *de1 = (const tf_dirent *)d1;
	const tf_dirent *de2 = (const tf_dirent *)d2;

	int ret = tf_cmp_dirent_by_type(de1, de2);

	if (ret == 0) {
		ret = -strcasecmp(de1->name, de2->name);
	}
	return ret;
}

static int tf_cmp_dirent_by_reverse_time(const void *d1, const void *d2)
{
	const tf_dirent *de1 = (const tf_dirent *)d1;
	const tf_dirent *de2 = (const tf_dirent *)d2;

	int ret = tf_cmp_dirent_by_type(de1, de2);

	if (ret == 0) {
		ret = de1->stamp - de2->stamp;
	}
	return ret;
}

static int tf_cmp_dirent_by_reverse_size(const void *d1, const void *d2)
{
	const tf_dirent *de1 = (const tf_dirent *)d1;
	const tf_dirent *de2 = (const tf_dirent *)d2;

	int ret = tf_cmp_dirent_by_type(de1, de2);

	if (ret == 0) {
		ret = (de1->size < de2->size) ? -1 : (de1->size == de2->size) ? 0 : 1;
	}
	return ret;
}

void tf_sort_dirents(tf_dir_entries *entries, int sort_type)
{
	switch (sort_type) {
		case TF_SORT_NAME:
			qsort(entries->entry, entries->count, sizeof(*entries->entry), tf_cmp_dirent_by_name);
			break;

		case TF_SORT_SIZE:
			qsort(entries->entry, entries->count, sizeof(*entries->entry), tf_cmp_dirent_by_size);
			break;

		case TF_SORT_TIME:
			qsort(entries->entry, entries->count, sizeof(*entries->entry), tf_cmp_dirent_by_time);
			break;

		case -TF_SORT_NAME:
			qsort(entries->entry, entries->count, sizeof(*entries->entry), tf_cmp_dirent_by_reverse_name);
			break;

		case -TF_SORT_SIZE:
			qsort(entries->entry, entries->count, sizeof(*entries->entry), tf_cmp_dirent_by_reverse_size);
			break;

		case -TF_SORT_TIME:
			qsort(entries->entry, entries->count, sizeof(*entries->entry), tf_cmp_dirent_by_reverse_time);
			break;

	}
}
