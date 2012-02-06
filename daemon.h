/* $Id: daemon.h,v 1.1.1.1 2005/11/23 10:25:07 steveb Exp $ */

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
#ifndef DAEMON_H
#define DAEMON_H

/**
 * Acquire a lock on the given pidfile, waiting up to 'block_seconds'
 * seconds for the lock to become available.
 * If successful, returns a file descriptor which must be closed to
 * release the lock (use daemon_release_lock()).
 * The the lock wasn't available within the given time, returns -1.
 * If an error occurs, returns -2.
 */
int daemon_acquire_lock(const char *pidfile, int block_seconds);
    
/** 
 * Releases a lock acquired by daemon_acquire_lock()
 */
int daemon_release_lock(int lock_fd);

/**
 * Function to create and lock the pid file for a daemon
 * process to ensure that it only runs once.
 * The lock is released (but the file is not removed)
 * when the process exits.
 *
 * Returns 0 if locked OK,
 * -1 if error
 * pid (> 0) if already locked.
 */ 
int daemon_lock_pid(const char *pidfile, int *savefd);

#endif
