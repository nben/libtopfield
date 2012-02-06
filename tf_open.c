/* $Id: tf_open.c,v 1.2 2005/11/23 10:54:43 steveb Exp $ */

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
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>

#include "usb_io.h"
#include "tf_open.h"
#include "usbutil.h"

#define TOPFIELD_VENDOR_ID 0x11DB
#define TOPFIELD_5000PVRT_ID 0x1000

/**
 * May return a pointer to a static buffer.
 */
static const char *lock_filename(int index)
{
	if (index == 0) {
		return TF_LOCKFILE;
	}
	else {
		static char filename[100];

		snprintf(filename, sizeof(filename), "%s.%d", TF_LOCKFILE, index);

		return filename;
	}
}

int topfield_open(tf_handle *tf, int index, tf_lock_t lock)
{
	memset(tf, 0, sizeof(*tf));
	tf->timeout = TF_DEFAULT_TIMEOUT;
	tf->tracefh = stderr;
	tf->lock_fd = -1;

	/* The lock filename MUST be compatible with puppy, otherwise
	 * the locking scheme will break and users will experience USB
	 * bus stalls due to Linux kernel bugs. */

	if (lock != TF_LOCK_NONE) {
		for (;;) {
			char buf[10];

			tf->lock_fd = open(lock_filename(index), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
			/*fprintf(stderr, "lock_fd=%d\n", tf->lock_fd);*/

			if (tf->lock_fd >= 0 && flock(tf->lock_fd, LOCK_EX | LOCK_NB) == 0) {
				/*fprintf(stderr, "locked OK, writing pid=%d\n", getpid());*/
				/* locked OK, so write our pid */
				sprintf(buf, "%d\n", getpid());
				write(tf->lock_fd, buf, strlen(buf));
				break;
			}

			/* Already locked, or we don't have enough privs */

			/* In case we managed to open it */
			close(tf->lock_fd);

			if (lock == TF_LOCK_HUP) {
				/* Already locked, so find pid */
				int fd;

				/* Don't try more than once */
				lock = TF_LOCK_STD;

				fd = open(lock_filename(index), O_RDONLY);
				/*fprintf(stderr, "Trying SIGHUP, fd=%d\n", fd);*/
				if (fd >= 0) {
					int pid = 0;
					int len = read(fd, buf, sizeof(buf) - 1); 
					/*fprintf(stderr, "len=%d\n", len);*/
					if (len > 0) {
						buf[len] = 0;
						pid = atoi(buf);
						/*fprintf(stderr, "buf=%s, pid=%d\n", buf, pid);*/
					}
					close(fd);

					if (pid > 0) {
						/*fprintf(stderr, "sending SIGHUP to pid=%d\n", pid);*/
						/*syslog(LOG_INFO, "Topfield is in use, trying to kick off pid %d", pid);*/

						if (kill(pid, SIGHUP) == 0) {
							/* That all worked, so wait a couple of seconds and try once more */
							/*fprintf(stderr, "kill OK, retrying\n");*/
							sleep(2);
							continue;
						}
						/*fprintf(stderr, "kill failed %m\n");*/
					}
				}
			}

			/*fprintf(stderr, "giving up\n");*/

			/* No go, so give up */
			return 1;
		}
	}

	tf->dev = open_usb_dev(TOPFIELD_VENDOR_ID, TOPFIELD_5000PVRT_ID, index);

    if (!tf->dev) {
		close(tf->lock_fd);
		tf->lock_fd = -1;
        return -1;
    }

	/* Allocate a buffer big enough for a send/reply packet */
	tf->buf = malloc(0x10000);

	return 0;
}

void topfield_close(tf_handle *tf)
{
	if (tf->dev) {
		close_usb_dev(tf->dev);
		tf->dev = 0;
		free(tf->buf);
		tf->buf = 0;
		close(tf->lock_fd);
	}
}

