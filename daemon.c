
/* $Id: daemon.c,v 1.1.1.1 2005/11/23 10:25:07 steveb Exp $ */

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

#include <syslog.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "daemon.h"

/* This is all based on code originally by Richard Stevens APUE */

static int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len)
{
	struct flock	lock;

	lock.l_type = type;		/* F_RDLCK, F_WRLCK, F_UNLCK */
	lock.l_start = offset;	/* byte offset, relative to l_whence */
	lock.l_whence = whence;	/* SEEK_SET, SEEK_CUR, SEEK_END */
	lock.l_len = len;		/* #bytes (0 means to EOF) */

	return( fcntl(fd, cmd, &lock) );
}

int daemon_release_lock(int lock_fd)
{
#ifdef DEBUG
	syslog(LOG_INFO, "Releasing lock on fd: %d", lock_fd);
#endif
	return(close(lock_fd));
}

int daemon_acquire_lock(const char *pidfile, int block_seconds)
{
	int		fd;
	int		val;
	char	buf[10];
	int		tries = 0;

	if ( (fd = open(pidfile, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
#ifdef DEBUG
		syslog(LOG_ERR, "Failed to open pid file '%s' for writing: %m", pidfile);
#endif
		return(-2);
	}
#ifdef DEBUG
	syslog(LOG_INFO, "acquire_lock(%s) opened lockfile - fd=%d", pidfile, fd);
#endif

	/* try and set a write lock on the entire file */
	while (lock_reg(fd, F_SETLK, F_WRLCK, 0, SEEK_SET, 0) < 0) {
		if (errno != EACCES && errno != EAGAIN) {
#ifdef DEBUG
			syslog(LOG_ERR, "Failed to lock pid file '%s': %m", pidfile);
#endif
			close(fd);
			return(-2);
		}
		if (tries++ >= block_seconds) {
#ifdef DEBUG
				syslog(LOG_ERR, "Failed to lock pid file '%s' after %d seconds: %m", pidfile, block_seconds);
#endif
			/* gracefully exit, daemon is already running */
			close(fd);
			return(-1);
		}

#ifdef DEBUG
		syslog(LOG_INFO, "acquire_lock(%s) failed to lock, waiting...", pidfile);
#endif

		/* Wait a moment and try again */
		sleep(1);
	}

#ifdef DEBUG
	syslog(LOG_INFO, "acquire_lock(%s) locked fd OK", pidfile);
#endif

	/* truncate to zero length, now that we have the lock */
	if (ftruncate(fd, 0) < 0) {
#ifdef DEBUG
		syslog(LOG_ERR, "Failed to truncate pid file '%s': %m", pidfile);
#endif
		close(fd);
		return(-2);
	}

	/* and write our process ID */
	sprintf(buf, "%d\n", getpid());
	if (write(fd, buf, strlen(buf)) != (int)strlen(buf)) {
#ifdef DEBUG
		syslog(LOG_ERR, "Failed to write to pid file '%s': %m", pidfile);
#endif
		close(fd);
		return(-2);
	}

#ifdef DEBUG
	syslog(LOG_INFO, "acquire_lock(%s) wrote pid=%d", pidfile, getpid());
#endif

	/* set close-on-exec flag for descriptor */
	/* make this a non-fatal error */
	if ((val = fcntl(fd, F_GETFD, 0)) < 0) {
#ifdef DEBUG
		syslog(LOG_ERR, "Failed to GETFD for pid file '%s': %m", pidfile);
#endif
	}
	else {
		val |= FD_CLOEXEC;
		if (fcntl(fd, F_SETFD, val) < 0) {
#ifdef DEBUG
			syslog(LOG_ERR, "Failed to SETFD for pid file '%s': %m", pidfile);
#endif
		}
	}

#ifdef DEBUG
	syslog(LOG_INFO, "acquire_lock(%s) returning fd=%d", pidfile, fd);
#endif

	return(fd);
}

/**
 * Returns 0 if locked OK,
 * -1 if error
 * pid if already locked.
 *
 * Stores the locking fd in *savefd if it is non-NULL
 */
int daemon_lock_pid(const char *pidfile, int *savefd)
{
	int fd = daemon_acquire_lock(pidfile, 0);

#ifdef DEBUG
	syslog(LOG_INFO, "lock_pid(%s) acquire_lock() returned %d", pidfile, fd);
#endif

	if (fd >= 0) {
		if (savefd) {
			*savefd = fd;
		}
		/* We MUST leave the file open to maintain the lock */
		return(0);
	}
	if (fd == -1) {
		int pid = 0;

		fd = open(pidfile, O_RDONLY);
		if (fd >= 0) {
			char buf[10];
			int len = read(fd, buf, sizeof(buf) - 1); 
			if (len > 0) {
				buf[len] = 0;
				pid = atoi(buf);
			}
			close(fd);
		}
#ifdef DEBUG
		syslog(LOG_INFO, "lock_pid(%s) already locked, pid=%d", pidfile, pid);
#endif
		/* If we can't determine which pid, fake it */
		return pid ?: 1;
	}
	return -1;
}
