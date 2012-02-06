/* $Id: tf_io.h,v 1.2 2006/01/05 04:25:10 msteveb Exp $ */

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
#ifndef TF_IO_H
#define TF_IO_H

/* Provides an interface to the Topfield 5000PVR USB functionality */

#include "tf_types.h"

#include "tf_open.h"

/**
 * This is the maximum allowed data size
 * for tf_cmd_put_data()
 */
#define MAX_PUT_SIZE (0xFFFF - 8 - 9)

/**
 * This is the maximum buffer which will be
 * returned by tf_cmd_get_next()
 */
#define MAX_GET_SIZE (0xFFFF - 8)

/**
 * Structure containing the results of tf_cmd_size()
 */
typedef struct {
	__u32 totalk;	/* Total KiB */
	__u32 freek;	/* Free KiB */
} tf_size_result;

/**
 * Structure representing a single directory entry.
 */
typedef struct {
	time_t stamp;	/* Timestamp */
	char type;		/* d=dir, f=file */
	__u64 size;		/* Length in bytes */
	char name[95];	/* Null terminated filename */
	__u32 attrib;	/* Currently always 0 */
} tf_dirent;

/**
 * Maximum number of directory entries which will
 * fit in one request packet
 */
#define MAX_DIR_ENTRIES 595

/**
 * Structure containg the result of tf_cmd_dir_first()/tf_cmd_dir_next()
 */
typedef struct {
	int count;		/* Number of valid entries in the entry[] array */
	tf_dirent entry[MAX_DIR_ENTRIES];	/* Directory entries */
} tf_dir_entries;

/**
 * Structure which is used to return the buffer
 * in tf_cmd_get_next()
 */
typedef struct {
	__u64 offset;	/* Offset of the data in the file */
	size_t size;	/* Length of this data */
	__u8 *data;		/* The data */
} tf_buffer;

/**
 * Possible error codes returned by all the tf_ functions
 */
#define TF_ERR_NONE         0		/* Success */
#define TF_ERR_DONE         1		/* Not really an error.
                                     * No more files in listing,
									 * or the transfer is complete */

/* These are all the Topfield defined errors as negative numbers */
#define TF_ERR_CRC         -1
#define TF_ERR_FAIL2       -2
#define TF_ERR_BADCMD      -3
#define TF_ERR_FAIL4       -4
#define TF_ERR_BLKSIZE     -5
#define TF_ERR_GENERR      -6
#define TF_ERR_NOMEM       -7

/* These are various internal errors */
#define	TF_ERR_UNEXPECTED  -100		/* Unexpected response packet */
#define	TF_ERR_IO          -101		/* I/O error (e.g. short read) */
#define	TF_ERR_NOCONN      -102		/* Not connected. topfield_open() did not succeed */

/**
 * These tf_cmd... functions are all synchronous.
 * The request is sent and the response received.
 * They all return 0 if OK or < 0 on error.
 * The error is also stored in tf->error.
 */

/**
 * Establishes initial communication with the topfield
 * after opening.
 * This puts the device into a known, good state.
 * Returns 0 if OK, or -1 on error.
 */
int tf_init(tf_handle *tf);

/**
 * Returns disk space total and free
 */
int tf_cmd_size(tf_handle *tf, tf_size_result *result);

/**
 * Cancels any outstanding request (e.g. put or get)
 */
int tf_cmd_cancel(tf_handle *tf);

/**
 * Sends a ready notification to the Topfield.
 * I don't know what this is good for!
 */
int tf_cmd_ready(tf_handle *tf);

/**
 * Reboot the Topfield.
 * Note that after this operation, you will need to
 * close the handle and reopen it. This is currently
 * not done automatically.
 */
int tf_cmd_reset(tf_handle *tf);

/**
 * Enable or disable turbo mode according to 'on'
 */
int tf_cmd_turbo(tf_handle *tf, int on);

/**
 * Delete the given file or directory.
 * The path is an absolute path name such as /ProgramFiles/Auto Start/test
 */
int tf_cmd_delete(tf_handle *tf, const char *path);

/**
 * Rename the source file to the destination file.
 * Both paths are absolute path names.
 */
int tf_cmd_rename(tf_handle *tf, const char *src, const char *dest);

/**
 * Create a directory with the given name.
 * The path is an absolute path name such as /DataFiles/tmp
 */
int tf_cmd_mkdir(tf_handle *tf, const char *path);

/**
 * These tf_cmd_... functions are all transactional.
 * Since the Topfield is a bit finicky, it is very important
 * that you complete the transaction appropriately (i.e. with
 * the appropriate done or cancel function) or else you may
 * no longer be able to talk to it until a reboot.
 */

/**
 * Returns a list of files in the given directory as specified
 * by 'path'. This should be an absolute path name such as \DataFiles
 * If the request succeeds, 0 (TF_ERR_NONE) will be returned
 * and the result is stored in *result.
 *
 * If there are no files, TF_ERR_NOFILES is returned.
 * (Note that this is > 0 while all other error codes are < 0)
 *
 * If you wish to cancel a file listing early, you must call tf_cmd_dir_cancel().
 */
int tf_cmd_dir_first(tf_handle *tf, const char *path, tf_dir_entries *result);

/**
 * Continues an in-progress file list operation.
 * Otherwise identical to tf_cmd_dir_first()
 */
int tf_cmd_dir_next(tf_handle *tf, tf_dir_entries *result);

/**
 * Cancels an in-progress file list operation (one that returned 0).
 * This need not be called for a file list operation which returned non-zero.
 */
int tf_cmd_dir_cancel(tf_handle *tf);

/**
 * Begins a file get operation for the file specified by the given
 * full path name.
 * The offset specifies the file offset to begin sending data.
 * If the successful, 0 (TF_ERR_NONE) is returned and the details of
 * the file are stored in *dirent.
 *
 * If this operation succeeds, you must call tf_cmd_get_cancel() at some
 * point unless you call tf_cmd_get_next() and it returns TF_ERR_DONE.
 */
int tf_cmd_get(tf_handle *tf, const char *path, __u64 offset, tf_dirent *dirent);

/**
 * Gets the next buffer of data from an in-progress file get operation.
 * If there is more data to get, this operation returns 0 (TF_ERR_NONE).
 * If all data has been retrieved, this operation returns TF_ERR_DONE.
 * Note that this is the ONLY case where you need not call tf_cmd_get_cancel().
 * Any other result indicates that the operation has failed and tf_cmd_get_cancel()
 * must be called.
 */
int tf_cmd_get_next(tf_handle *tf, tf_buffer *buf);

/**
 * Cancel and in-progress or failed file get operation.
 */
int tf_cmd_get_cancel(tf_handle *tf);

/**
 * Begins a file put operation for the file specified by the given
 * full path name.
 * The size specifies the size of the file being transferred (but
 * there seems to no ill effect from setting this to 0).
 * The stamp is the timestamp for the file. It should be >= 1 Jan 2003.
 * The offset specifies the byte offset in the target file where data will
 * be stored (if the file exists).
 *
 * Similarly to tf_cmd_get(), if this operation succeeds, tf_cmd_put_cancel()
 * must be called to cancel the operation, or tf_cmd_put_done() may be called
 * if the tranfer is complete.
 */
int tf_cmd_put(tf_handle *tf, const char *path, __u64 size, time_t stamp, __u64 offset);

/**
 * Transfers the data at the the given remote file file offset.
 * Note: It may be possible to write the data non-sequentially, but
 *       I haven't tried!
 */
int tf_cmd_put_data(tf_handle *tf, __u64 offset, void *buffer, size_t len);

/**
 * Indicates that the file transfer is complete.
 */
int tf_cmd_put_done(tf_handle *tf);

/**
 * If the file transfer is to be finished early (possibly because a
 * tf_cmd_put_data() call failed), this function must be called to
 * cancel the transfer.
 */
int tf_cmd_put_cancel(tf_handle *tf);

/**
 * Returns a string describing the given error number.
 */
const char *tf_strerror(int error);

/**
 * These commands are asynchronous.
 * No result is waited for.
 */

/**
 * Sends a success message
 */
int tf_send_success(tf_handle *tf);

/**
 * Sends a fail message with the given reason code.
 * Note that if you use the TF_ERR_... codes, you must
 * negate them first.
 * e.g. tf_send_fail(tf, -TF_ERR_CRC)
 */
int tf_send_fail(tf_handle *tf, int reason);

#endif
