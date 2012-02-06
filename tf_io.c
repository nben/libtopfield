
/* $Id: tf_io.c,v 1.3 2005-12-13 02:47:04 steveb Exp $ */

/*
  Copyright (c) 2005 Steve Bennett <msteveb at ozemail.com.au>

  Based in part on code - copyright (C) 2004 Peter Urbanec <toppy at urbanec.net>

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
#include "usb_io.h"

#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>

#include "tf_io.h"
#include "tf_bytes.h"
#include "crc16.h"
#include "mjd.h"

/*#define DEBUG*/
/*#define DEBUG_DUMP*/

#ifdef DEBUG
#define DEBUG_LOG(A...) syslog(LOG_INFO, A); fprintf(stderr, A); fputc('\n', stderr)
#else
#define DEBUG_LOG(A...)
#endif

/* The maximum packet size used by the Toppy. This happens to be an
   odd number, which could cause issues when the Topfield specific
   byte swapping is applied. It's best to ensure that transmitted
   packets contain an even number of bytes that is smaller than this
   value.
*/
#define MAXIMUM_PACKET_SIZE 0xFFFFL

/* The size of every packet header. */
#define PACKET_HEAD_SIZE 8

/* Format of a Topfield protocol packet */
typedef struct
{
	__u16 length;
	__u16 crc;
	__u32 cmd;
	__u8 data[MAXIMUM_PACKET_SIZE - PACKET_HEAD_SIZE];
	__u8 dummy;	/* extra dummy byte sometimes gets sent for padding */
} __attribute__ ((packed)) tf_packet_t;

/* Possible values for tf_typefile_t->filetype */
#define TYPE_DIR  1
#define TYPE_FILE 2

/* Topfield file descriptor data structure. */
typedef struct
{
	struct tf_datetime stamp;
	__u8 filetype;
	__u64 size;
	__u8 name[95];
	__u8 unused;
	__u32 attrib;
} __attribute__ ((packed)) tf_typefile_t;

/* Topfield command codes */
#define TF_MSG_FAIL                  0x0001
#define TF_MSG_SUCCESS               0x0002
#define TF_MSG_CANCEL                0x0003

#define TF_MSG_READY                 0x0100
#define TF_MSG_RESET                 0x0101
#define TF_MSG_TURBO                 0x0102

#define TF_MSG_HDD_SIZE              0x1000
#define TF_MSG_HDD_SIZE_RESULT       0x1001

#define TF_MSG_HDD_DIR               0x1002
#define TF_MSG_HDD_DIRENT            0x1003
#define TF_MSG_HDD_DIREND            0x1004

#define TF_MSG_HDD_DELETE            0x1005
#define TF_MSG_HDD_RENAME            0x1006
#define TF_MSG_HDD_MKDIR             0x1007

#define TF_MSG_HDD_FILE_SEND         0x1008
#define TF_MSG_HDD_FILE_START        0x1009
#define TF_MSG_HDD_FILE_DATA         0x100A
#define TF_MSG_HDD_FILE_END          0x100B

/* Direction for TF_MSG_HDD_FILE_SEND */
#define DIR_PUT 0
#define DIR_GET 1

static const char *tf_command_name(long cmd);
static void print_packet(FILE *fh, const char *prefix, int trace_level, tf_packet_t *packet);

#ifdef DEBUG_DUMP
static void dump_hex_buf(FILE *fh, const __u8 *buf, size_t len)
{
	int i;

	for (i = 0; i < len; ++i) {
		if (i && (i % 32) == 0) {
			fprintf(fh, "\n");
		}
		fprintf(fh, " %02x", buf[i]);
	}
	fprintf(fh, "\n");
}
#endif


/**
 * Initialise a request packet with the given command.
 */
static void tf_req_init(tf_packet_t *req, __u32 cmd)
{
	/* We keep the length in host order right until the end
	 * so that it is easy to work with
	 */
	req->length = 8;
	req->crc = 0;
	put_u32(&req->cmd, cmd);
	req->dummy = 0;
}

/**
 * Finalises a request packet after it has been initialised
 * and then filled in with the appropriate tf_req_put...() calls.
 * Byteswaps the 
 */
static void tf_req_done(tf_handle *tf, tf_packet_t *req)
{
	/* Swap the length. Calculate the CRC. Byteswap the whole packet */
	__u16 len = req->length;
	int pad = 0;

	put_u16(&req->length, len);

	put_u16(&req->crc, crc16_ansi(0, &req->cmd, len - 4));

	print_packet(tf->tracefh, ">>", tf->trace_level, req);

	if (len % 2 == 1) {
		/* If the packet size would be odd, padd it with an extra byte */
		pad = 1;
		((unsigned char *)req)[len] = 0;
	}

	byte_swap(req, len + pad);
}

static void tf_req_put8(tf_packet_t *req, __u8 value)
{
	__u8 *b = (__u8 *)req;

	b[req->length] = value;
	req->length++;
}

static void tf_req_put16(tf_packet_t *req, __u16 value)
{
	__u8 *b = (__u8 *)req;

	put_u16(b + req->length, value);
	req->length += 2;
}

static void tf_req_put32(tf_packet_t *req, __u32 value)
{
	__u8 *b = (__u8 *)req;

	put_u32(b + req->length, value);
	req->length += 4;
}

static void tf_req_put64(tf_packet_t *req, __u64 value)
{
	__u8 *b = (__u8 *)req;

	put_u64(b + req->length, value);
	req->length += 8;
}

static void tf_req_putdata(tf_packet_t *req, const void *data, size_t len)
{
	__u8 *b = (__u8 *)req;

	memcpy(b + req->length, data, len);
	req->length += len;
}

/**
 * Writes a filename, possibly preceded by a two-byte length
 * and padded to an even boundary.
 *
 * Forward slashes are translated to backslashes
 */
static void tf_req_putfilename(tf_packet_t *req, const char *str, int addlength)
{
	/* When writing a filename, we need to include a null terminator
	 * and then ensure that if we would end on an odd byte boundary,
	 * include an extra null padding byte
	 */

	/* Add one so we pick up the null terminator */
	int len = strlen(str) + 1;
	int pad = 0;

	/* Now we need to add 1 to the length if we wouldn't end
	 * up on an even boundary
	 */
	if ((len + req->length) & 1) {
		pad = 1;
	}
	if (addlength) {
		tf_req_put16(req, len + pad);
	}
	while (len--) {
		tf_req_put8(req, *str == '/' ? '\\' : *str);
		str++;
	}
	if (pad) {
		tf_req_put8(req, 0);
	}
}

/**
 * Sends the packet to the device.
 * Returns 0 if OK or <0 on error.
 * Stores the error in tf->error.
 */
static int tf_send(tf_handle *tf, const tf_packet_t *req)
{
	if (tf->dev) {
		int len = get_u16_raw(&req->length);
		int pad = 0;
		if (len % 64 == 0) {
			/* Need to pad multiple-of-64 packets to prevent
			 * a topfield protocol bug
			 */
			pad = 2;
			DEBUG_LOG("tf_send(%s) padding len=%d with %d bytes",
				tf_command_name(get_u32_raw(&req->cmd)), len, pad);
		}
		else if (len % 2) {
			/* Need to pad odd packets */
			pad = 1;
		}
		if (pad) {
			len += pad;
			/* Pad with zero bytes for safety */
			/*memset(((unsigned char *)req->data) + len, 0, pad);*/
		}
		DEBUG_LOG("usb_bulk_write(len=%d)", len);
#ifdef DEBUG_DUMP
		dump_hex_buf(tf->tracefh, (void *)req, len);
#endif
		int ret = usb_bulk_write(tf->dev, 0x01, (void *)req, len, tf->timeout);

		tf->error = (ret == len) ? TF_ERR_NONE : TF_ERR_IO;
	}
	else {
		tf->error = TF_ERR_NOCONN;
	}

	return tf->error;
}

static int tf_get_response(tf_handle *tf, tf_packet_t *reply)
{
	int ret;

	tf->error = 0;

	if (!tf->dev) {
		tf->error = TF_ERR_NOCONN;
		return tf->error;
	}

	ret = usb_bulk_read(tf->dev, 0x82, (void *)reply, sizeof(*reply), tf->timeout);

	if (ret < PACKET_HEAD_SIZE) {
		if (ret >= 0) {
			DEBUG_LOG("usb_bulk_read() returned ret=%d < 8", ret);
		}
		tf->error = TF_ERR_IO;
	}
	else {
		__u16 len = get_u16_raw(&reply->length);

		if (ret < len) {
			DEBUG_LOG("usb_bulk_read() returned ret=%d < len=%d", ret, len);
			tf->error = TF_ERR_IO;
		}
		else {
			/* Note: we need to byte swap an extra byte if odd */
			byte_swap(reply, len + (len % 2));

			print_packet(tf->tracefh, "<<", tf->trace_level, reply);

			if (!tf->nocrc) {
				__u16 crc;
				__u16 calc_crc;
				crc = get_u16(&reply->crc);
				calc_crc = crc16_ansi(0, &(reply->cmd), len - 4);

				if (crc != calc_crc) {
					DEBUG_LOG("reply crc=%04X != calc crc=%04X", crc, calc_crc);
					tf->error = TF_ERR_CRC;
				}
			}

			/* Make it easier to read */
			reply->length = len;
			reply->cmd = get_u32(&reply->cmd);
		}
	}
	return tf->error;
}

/**
 * Send the command and wait for a SUCCESS response.
 * Returns 0 if OK.
 * If a FAIL is received, returns the failure code (as a negative number)
 * If anything else is received, returns -100.
 * Stores the result in tf->error as well.
 */
static int tf_cmd(tf_handle *tf, tf_packet_t *req)
{
	int ret = tf_send(tf, req);
	if (ret == 0) {
		tf_packet_t reply;
		ret = tf_get_response(tf, &reply);
		if (ret == 0) {
			if (reply.cmd == TF_MSG_FAIL) {
				ret = tf->error = -get_u32(reply.data);
			}
			else if (reply.cmd != TF_MSG_SUCCESS) {
				DEBUG_LOG("tf_cmd(): reply.cmd = unexpected %d", reply.cmd);
				ret = tf->error = TF_ERR_UNEXPECTED;
			}
		}
	}
	else {
		DEBUG_LOG("tf_cmd(): tf_send() returned %d", ret);
	}
	return ret;
}

int tf_init(tf_handle *tf)
{
	/* Send a cancel in case anything is outstanding */
	if (tf_cmd_cancel(tf) == 0) {
		return 0;
	}

	/* Often the first cancel fails, so try again */
	if (tf_cmd_cancel(tf) == 0) {
		return 0;
	}

	/* If that fails, ... */
	/* First send a fail */
	tf_send_fail(tf, 2);

	/* Now a success */
	tf_send_success(tf);

	/* Try a ready command */
	if (tf_cmd_ready(tf) == 0) {
		return 0;
	}

	/* And finally, one more cancel */
	if (tf_cmd_cancel(tf) == 0) {
		return 0;
	}

	/* Give up */
	return -1;
}

int tf_cmd_size(tf_handle *tf, tf_size_result *result)
{
	tf_packet_t req;
	int ret;

	tf_req_init(&req, TF_MSG_HDD_SIZE);
	tf_req_done(tf, &req);

	ret = tf_send(tf, &req);
	if (ret == 0) {
		tf_packet_t reply;

		ret = tf_get_response(tf, &reply);
		if (ret == 0) {
			if (reply.cmd == TF_MSG_HDD_SIZE_RESULT) {
				const tf_size_result *r = (const tf_size_result *)&reply.data;

				result->totalk = get_u32(&r->totalk);
				result->freek = get_u32(&r->freek);
			}
			else {
				ret = TF_ERR_UNEXPECTED;
			}
		}
	}
	return ret;
}

int tf_cmd_cancel(tf_handle *tf)
{
	tf_packet_t req;

	tf_req_init(&req, TF_MSG_CANCEL);
	tf_req_done(tf, &req);

	return tf_cmd(tf, &req);
}

int tf_cmd_ready(tf_handle *tf)
{
	tf_packet_t req;

	tf_req_init(&req, TF_MSG_READY);
	tf_req_done(tf, &req);

	return tf_cmd(tf, &req);
}

int tf_cmd_reset(tf_handle *tf)
{
	tf_packet_t req;

	tf_req_init(&req, TF_MSG_RESET);
	tf_req_done(tf, &req);

	return tf_cmd(tf, &req);
}

int tf_cmd_turbo(tf_handle *tf, int on)
{
	tf_packet_t req;

	tf_req_init(&req, TF_MSG_TURBO);
	tf_req_put32(&req, on);
	tf_req_done(tf, &req);

	return tf_cmd(tf, &req);
}

int tf_cmd_delete(tf_handle *tf, const char *path)
{
	tf_packet_t req;

	tf_req_init(&req, TF_MSG_HDD_DELETE);
	tf_req_putfilename(&req, path, 0);
	tf_req_done(tf, &req);

	return tf_cmd(tf, &req);
}

int tf_cmd_mkdir(tf_handle *tf, const char *path)
{
	tf_packet_t req;

	tf_req_init(&req, TF_MSG_HDD_MKDIR);
	tf_req_putfilename(&req, path, 1);
	tf_req_done(tf, &req);

	return tf_cmd(tf, &req);
}

int tf_cmd_rename(tf_handle *tf, const char *src, const char *dest)
{
	tf_packet_t req;

	tf_req_init(&req, TF_MSG_HDD_RENAME);
	tf_req_putfilename(&req, src, 1);
	tf_req_putfilename(&req, dest, 1);
	tf_req_done(tf, &req);

	return tf_cmd(tf, &req);
}

static int unpack_dirent(tf_dirent *d, const tf_typefile_t *typefile)
{
	d->stamp = tfdt_to_time(&typefile->stamp);
	d->type = (typefile->filetype == TYPE_DIR) ? 'd' : 'f';
	d->size = get_u64(&typefile->size);
	strcpy(d->name, (char *)typefile->name);
	d->attrib = get_u16(&typefile->attrib);

	return 0;
}

static int get_dirents(tf_handle *tf, tf_dir_entries *result)
{
	tf_packet_t reply;

	int ret = tf_get_response(tf, &reply);
	if (ret == 0) {
		if (reply.cmd == TF_MSG_HDD_DIRENT) {
			/* Unpack each entry into the array */
			const tf_typefile_t *typefile = (const tf_typefile_t *)reply.data;
			int i;

			result->count = (reply.length - PACKET_HEAD_SIZE) / sizeof(tf_typefile_t);

			for (i = 0; i < result->count; i++) {
				unpack_dirent(&result->entry[i], &typefile[i]);
			}
		}
		else if (reply.cmd == TF_MSG_HDD_DIREND) {
			/* No more files */
			tf_send_success(tf);

			ret = TF_ERR_DONE;
		}
		else {
			ret = TF_ERR_UNEXPECTED;
		}
	}
	return ret;
}

/**
 * Returns 0 if OK, 1 if no more files.
 */
int tf_cmd_dir_first(tf_handle *tf, const char *path, tf_dir_entries *result)
{
	tf_packet_t req;
	int ret;

	tf_req_init(&req, TF_MSG_HDD_DIR);
	tf_req_putfilename(&req, path, 0);
	tf_req_done(tf, &req);

	ret = tf_send(tf, &req);

	if (ret == 0) {
		ret = get_dirents(tf, result);
	}
	return ret;
}

int tf_cmd_dir_next(tf_handle *tf, tf_dir_entries *result)
{
	int ret = tf_send_success(tf);
	if (ret == 0) {
		ret = get_dirents(tf, result);
	}
	return ret;
}

int tf_cmd_dir_cancel(tf_handle *tf)
{
	/* Say we don't want any more results */
	return tf_cmd_cancel(tf);
}

int tf_cmd_put(tf_handle *tf, const char *path, __u64 size, time_t stamp, __u64 offset)
{
	tf_packet_t req;
	int ret;

	tf_req_init(&req, TF_MSG_HDD_FILE_SEND);
	tf_req_put8(&req, DIR_PUT);
	tf_req_putfilename(&req, path, 1);
	tf_req_put64(&req, offset);
	tf_req_done(tf, &req);

	ret = tf_send(tf, &req);

	if (ret == 0) {
		tf_packet_t reply;

		ret = tf_get_response(tf, &reply);

		if (ret == 0) {
			/* Now we expect a SUCCESS response */

			if (reply.cmd == TF_MSG_SUCCESS) {
				/* Good, now send a FILE_START */
				tf_typefile_t typefile;

				/* Fill in a few details */
				time_to_tfdt(stamp, &typefile.stamp);
				typefile.filetype = TYPE_FILE;
				typefile.size = size;
				snprintf((char *)typefile.name, sizeof(typefile.name), "%s", path);
				typefile.unused = 0;
				typefile.attrib = 0;

				tf_req_init(&req, TF_MSG_HDD_FILE_START);
				tf_req_putdata(&req, &typefile, sizeof(typefile));
				tf_req_done(tf, &req);

				ret = tf_send(tf, &req);

				DEBUG_LOG("tf_send() file_start returned ret=%d", ret);

				if (ret == 0) {
					ret = tf_get_response(tf, &reply);

					DEBUG_LOG("tf_send() file_start get_response() returned ret=%d", ret);

					if (ret == 0 && reply.cmd != TF_MSG_SUCCESS) {
						ret = TF_ERR_UNEXPECTED;
					}
				}
			}
			else if (reply.cmd == TF_MSG_FAIL) {
				/* REVISIT: Should this be something else? */
				/*ret = TF_ERR_DONE;*/
				ret = -get_u32(reply.data);
			}
			else {
				ret = TF_ERR_UNEXPECTED;
			}
		}
		else {
			DEBUG_LOG("tf_cmd_put() get_response failed, ret=%d", ret);
		}
	}
	else {
		DEBUG_LOG("tf_cmd_put() failed to send, ret=%d", ret);
	}

	return ret;
}

int tf_cmd_put_data(tf_handle *tf, __u64 offset, void *buffer, size_t len)
{
	tf_packet_t req;
	int ret;

	tf_req_init(&req, TF_MSG_HDD_FILE_DATA);
	tf_req_put64(&req, offset);
	tf_req_putdata(&req, buffer, len);
	tf_req_done(tf, &req);

	ret = tf_send(tf, &req);
	if (ret == 0) {
		tf_packet_t reply;

		/* REVISIT: Use a longer timeout for puts */
		int timeout = tf->timeout;
		tf->timeout = timeout * 2;

		ret = tf_get_response(tf, &reply);

		tf->timeout = timeout;

		if (ret == 0) {
			/* Now we expect a SUCCESS response */
			if (reply.cmd != TF_MSG_SUCCESS) {
				ret = TF_ERR_UNEXPECTED;
			}
		}
		else {
			DEBUG_LOG("tf_cmd_put_data() get_response() failed, ret=%d", ret);
		}
	}
	else {
		DEBUG_LOG("tf_cmd_put_data() failed to send, ret=%d", ret);
	}
	return ret;
}

int tf_cmd_put_done(tf_handle *tf)
{
	tf_packet_t req;
	int ret;

	tf_req_init(&req, TF_MSG_HDD_FILE_END);
	tf_req_done(tf, &req);

	ret = tf_send(tf, &req);
	if (ret == 0) {
		tf_packet_t reply;

		ret = tf_get_response(tf, &reply);
		if (ret == 0 && reply.cmd != TF_MSG_SUCCESS) {
			ret = TF_ERR_UNEXPECTED;
		}
		if (ret != 0) {
			/* This didn't succeed, so send a cancel */
			tf_cmd_cancel(tf);
		}
	}
	return ret;
}

int tf_cmd_put_cancel(tf_handle *tf)
{
	/* It helps to send cancel twice */
	tf_cmd_cancel(tf);
	sleep(1);

	return tf_cmd_cancel(tf);
}

int tf_cmd_get(tf_handle *tf, const char *path, __u64 offset, tf_dirent *dirent)
{
	tf_packet_t req;
	int ret;

	tf_req_init(&req, TF_MSG_HDD_FILE_SEND);
	tf_req_put8(&req, DIR_GET);
	tf_req_putfilename(&req, path, 1);
	tf_req_put64(&req, offset);
	tf_req_done(tf, &req);

	ret = tf_send(tf, &req);

	if (ret == 0) {
		tf_packet_t reply;

		ret = tf_get_response(tf, &reply);

		if (ret == 0) {

			/*printf("tf_cmd_get() got response %04X\n", reply.cmd);*/

			/* Now we expect a HDD_FILE_START response */

			if (reply.cmd == TF_MSG_HDD_FILE_START) {
				/* Good, remember this info */
				unpack_dirent(dirent, (const tf_typefile_t *)reply.data);
				/*printf("tf_cmd_get() got start, returning 0\n");*/
			}
			else if (reply.cmd == TF_MSG_FAIL) {
				/*printf("tf_cmd_get() got fail\n");*/
				/* REVISIT: Should this be something else? */
				/*ret = TF_ERR_DONE;*/
				ret = -get_u32(reply.data);
			}
			else {
				ret = TF_ERR_UNEXPECTED;
			}
		}
	}

	return ret;
}

int tf_cmd_get_next(tf_handle *tf, tf_buffer *buf)
{
	/* Send success and wait for the packet to come back */
	int ret = 0;
	
	if (!tf->pending) {
		ret = tf_send_success(tf);
	}

	tf->pending = 0;

	if (ret == 0) {
		/* We use the buffer in the handle so we don't need to copy data more than once */
		tf_packet_t *reply = (tf_packet_t *)tf->buf;

		ret = tf_get_response(tf, reply);

		if (ret == 0) {
			if (reply->cmd == TF_MSG_HDD_FILE_END) {
				DEBUG_LOG("tf_cmd_get_next() got EOF");

				/* All done. */
				tf_send_success(tf);

				ret = TF_ERR_DONE;
			}
			else if (reply->cmd == TF_MSG_HDD_FILE_DATA) {
				/* Send a success response right away so we don't hold
				 * things up
				 */
				if (tf_send_success(tf) == 0) {
					tf->pending = 1;
				}

				__u64 off = get_u64(reply->data);
				__u16 len = reply->length - (PACKET_HEAD_SIZE + 8);

				buf->offset = off;
				buf->size = len;
				buf->data = &reply->data[8];

				ret = 0;
			}
			else if (reply->cmd == TF_MSG_FAIL) {
				/* Assume a CRC failure */
				DEBUG_LOG("tf_cmd_get_next() got FAIL -- assuming I/O error");
				ret = TF_ERR_IO;
			}
			else {
				DEBUG_LOG("tf_cmd_get_next() got unexpected packet");
				ret = TF_ERR_UNEXPECTED;
			}
		}
		else {
			DEBUG_LOG("tf_cmd_get_next() failed to get response");
		}
	}
	else {
		DEBUG_LOG("tf_cmd_get_next() failed to send success");
	}

	tf->error = ret;

	return ret;
}

int tf_cmd_get_cancel(tf_handle *tf)
{
	int i;
	int ret = 0;

	if (tf->pending) {
		tf_packet_t reply;

		tf->pending = 0;

		/* Get and discard any pending response */
		tf_get_response(tf, &reply);
	}


	/* Now sometimes the cancel doesn't work the first time, 
	 * so give it a few tries!
	 */
	for (i = 0; i < 5; i++) {
		tf_packet_t reply;

		/* First send a FAIL */
		tf_send_fail(tf, TF_ERR_CRC);

		if ((ret = tf_cmd_cancel(tf)) == 0) {
			break;
		}

		/* Did not succeed, so wait a moment and try to get the response again.
		 * This seems to give the most success since the topfield is slow
		 * to respond when cancelling a get
		 */
		sleep(2);

		ret = tf_get_response(tf, &reply);
		if (ret == 0) {
			break;
		}
	}

	if (ret != 0) {
		DEBUG_LOG("tf_cmd_get_cancel() failed after 5 attempts");
	}

	return ret;
}

/**
 * Returns a string describing the given error number.
 */
const char *tf_strerror(int error)
{
	switch (error) {
		case TF_ERR_NONE: return "No error";
		case TF_ERR_DONE: return "No more files";
		case TF_ERR_CRC: return "CRC error";
		case TF_ERR_FAIL2: return "Unknown command (2)";
		case TF_ERR_FAIL4: return "Unknown command (4)";
		case TF_ERR_BADCMD: return "Invalid command";
		case TF_ERR_BLKSIZE: return "Invalid block size";
		case TF_ERR_GENERR: return "Unknown error while running";
		case TF_ERR_NOMEM: return "Memory is full";
		case TF_ERR_UNEXPECTED: return "Unexpected response";
		case TF_ERR_IO: return "I/O error";
		case TF_ERR_NOCONN: return "Not connected";
		default: return "Unknown error";
	}
}

int tf_send_success(tf_handle *tf)
{
	static __u8 success_packet[8] = {
		0x08, 0x00, 0x81, 0xc1, 0x00, 0x00, 0x02, 0x00
	};

	if (tf->trace_level > 0) {
		fprintf(tf->tracefh, "(%ld) >>: cmd=SUCCESS(0x0002), len=8\n", time(0));
	}

	return tf_send(tf, (const tf_packet_t *)success_packet);
}

int tf_send_fail(tf_handle *tf, int reason)
{
	tf_packet_t req;

	tf_req_init(&req, TF_MSG_FAIL);
	tf_req_put32(&req, reason);
	tf_req_done(tf, &req);

	return tf_send(tf, &req);
}

/**
 * Prints the contents of the given packet to 'fh'.
 * If 'fh' is NULL, no trace is output.
 * If the trace level is 0, no trace is output.
 * A trace level of 1 outputs only the first 64 bytes of the packet.
 * A trace level of 1 outputs the entire packet.
 * 
 * It is assumed that the packet is in "host" byte order, not USB/Topfield
 * byte order.
 */
static void print_packet(FILE *fh, const char *prefix, int trace_level, tf_packet_t *packet)
{
	if (trace_level > 0 && fh) {
		int i;
		__u16 len = get_u16(&packet->length);
		__u16 crc = get_u16(&packet->crc);
		__u32 cmd = get_u32(&packet->cmd);
		__u8 *d = (__u8 *) packet->data;
		int num = trace_level == 1 ? 64 : len - 8;

		fprintf(fh, "(%ld) %s: cmd=%s(0x%04X), len=%d, crc=0x%04X", time(0), prefix, tf_command_name(cmd), cmd, len, crc);
		/*fprintf(fh, "\n%02X %02X %04X", len, crc, cmd);*/
		for (i = 0; i < num && i < len - 8; ++i) {
			if ((i % 32) == 0) {
				fprintf(fh, "\n");
			}
			fprintf(fh, " %02x", d[i]);
		}
		fprintf(fh, "\n");
	}
}

/**
 * Returns the name of the topfield command
 */
static const char *tf_command_name(long cmd)
{
	static char buf[64];

	switch (cmd) {
		case TF_MSG_FAIL: return "FAIL";
		case TF_MSG_SUCCESS: return "SUCCESS";
		case TF_MSG_CANCEL: return "CANCEL";

		case TF_MSG_READY: return "READY";
		case TF_MSG_RESET: return "RESET";
		case TF_MSG_TURBO: return "TURBO";

		case TF_MSG_HDD_SIZE: return "SIZE";
		case TF_MSG_HDD_SIZE_RESULT: return "SIZE_RESULT";

		case TF_MSG_HDD_DIR: return "DIR";
		case TF_MSG_HDD_DIRENT: return "DIRENT";
		case TF_MSG_HDD_DIREND: return "DIREND";

		case TF_MSG_HDD_DELETE: return "DELETE";
		case TF_MSG_HDD_RENAME: return "RENAME";
		case TF_MSG_HDD_MKDIR: return "MKDIR";

		case TF_MSG_HDD_FILE_SEND: return "FILE_SEND";
		case TF_MSG_HDD_FILE_START: return "FILE_START";
		case TF_MSG_HDD_FILE_DATA: return "FILE_DATA";
		case TF_MSG_HDD_FILE_END: return "FILE_END";
	}

	sprintf(buf, "UNKNOWN(%04lX)", cmd);
	return buf;
}
