
/* $Id: tf_fwio.c,v 1.1 2006/01/05 04:25:10 msteveb Exp $ */

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
#include <time.h>

#include "tf_fwio.h"
#include "tf_bytes.h"

/*#define DEBUG*/
/*#define DEBUG_DUMP*/

#ifdef DEBUG
#define DEBUG_LOG(A...) syslog(LOG_INFO, A); fprintf(stderr, A); fputc('\n', stderr)
#else
#define DEBUG_LOG(A...)
#endif

/* This is the maximum we can send in a single usb packet */
#define MAX_SEND_SIZE 0x4000

/* The size of every packet header. */
#define PACKET_HEAD_SIZE 8

/* Format of a Topfield protocol packet */
typedef struct
{
    __u8 tofi[4];	/* ToFi */
	__u8 seq;
	__u8 cmd;
	__u16 length;
    __u8 data[MAX_DATA_SIZE + 1];	/* Add one byte for the crc */
} __attribute__ ((packed)) tf_packet_t;

/* Topfield command codes */
#define TF_FW_ID                     0x02
#define TF_FW_PC_TO_STB              0x01
#define TF_FW_STB_TO_PC              0x03
#define TF_FW_REQ_DATA               0x04
#define TF_FW_DATA                   0x05
#define TF_FW_END                    0x06
#define TF_FW_REBOOT                 0x0B

static const char *tf_command_name(__u8 cmd);
static void print_packet(FILE *fh, const char *prefix, int trace_level, tf_packet_t *packet);

/**
 * Initialise a request packet with the given command and sequence number.
 */
static void tf_req_init(tf_packet_t *req, __u8 cmd, __u8 seq)
{
	/* We keep the length in host order right until the end
	 * so that it is easy to work with
	 */
	memset(req, 0, sizeof(*req));
	memcpy(req->tofi, "ToFi", 4);
	req->cmd = cmd;
	req->seq = seq;
}

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

static __u8 calc_checksum(const __u8 *buf, size_t len)
{
	__u8 sum = 0x00;
	while (len--) {
		/*printf("cksum: %02X\n", *buf);*/
		sum += *buf++;
	}
	/*printf("  => sum=%02X\n", sum);*/
	return sum;
}

/**
 * Finalises a request packet after it has been initialised
 * and then filled in with the appropriate tf_req_put...() calls.
 * Byteswaps the 
 */
static void tf_req_done(tf_handle *tf, tf_packet_t *req)
{
	/* Swap the length. Calculate and store the checksum. Byteswap the whole packet */
	__u16 len = req->length;

	put_u16(&req->length, len);

    req->data[len] = calc_checksum(&req->seq, len + 4);
	print_packet(tf->tracefh, ">>", tf->trace_level, req);
}

static void tf_req_putdata(tf_packet_t *req, const void *data, size_t len)
{
	memcpy(req->data + req->length, data, len);
	req->length += len;
}

/**
 * Sends the packet to the device.
 * Returns 0 if OK or <0 on error.
 * Stores the error in tf->error.
 */
static int tf_send(tf_handle *tf, const tf_packet_t *req)
{
	if (tf->dev) {
		unsigned char *data;
		int len = PACKET_HEAD_SIZE + get_u16(&req->length) + 1;
		int pad = 0;

		if (len % 2 != 0) {
			/* Need to pad odd size packet */
			pad = 1;
		}
		if (((len + pad) % 0x200) == 0) {
			/* For some reason the toppy doesn't like this size.
			 * send 2 extra bytes
			 */
			/* not sure we want to pad if we are at the max send size */
			if (pad + len != MAX_SEND_SIZE) {
				DEBUG_LOG("Padding len=%d with 2 extra bytes", len);
				pad += 2;
			}
		}
		if (pad) {
			DEBUG_LOG("Padding len=%d with %d bytes", len, pad);

			len += pad;

			/* Pad with zero bytes for safety */
			/*memset(((unsigned char *)req->data) + len, 0, pad);*/
		}
		/* The bootloader will only accept 0x4000 bytes at a time */
		tf->error = 0;
		data = (unsigned char *)req;
		do {
			size_t tosend = len;

			if (tosend > MAX_SEND_SIZE) {
				tosend = MAX_SEND_SIZE;
			}
			int ret = usb_bulk_write(tf->dev, 0x01, (void *)data, tosend, tf->timeout);

			DEBUG_LOG("usb_bulk_write(endpoint=0x01, len=%d, timeout=%dms)", tosend, tf->timeout);
#ifdef DEBUG_DUMP
			dump_hex_buf(stderr, (__u8 *)data, tosend);
#endif
			DEBUG_LOG("usb_bulk_write() returned %d", ret);
			if (ret != tosend) {
				tf->error = -1;
				break;
			}
			len -= tosend;
			data += tosend;
		} while (len);
	}
	else {
		tf->error = -2;
	}

	DEBUG_LOG("tf_send() returning error %d", tf->error);

	return tf->error;
}

static int tf_get_response(tf_handle *tf, tf_packet_t *reply)
{
	int ret;

	if (!tf->dev) {
		tf->error = -2;
		return tf->error;
	}

	ret = usb_bulk_read(tf->dev, 0x82, (void *)reply, sizeof(*reply), tf->timeout);
	DEBUG_LOG("usb_bulk_read(endpoint=0x82, len=%d, timeout=%dms) returned %d\n", sizeof(*reply), tf->timeout, ret);
#ifdef DEBUG_DUMP
	if (ret > 0) {
		dump_hex_buf(stderr, (__u8 *)reply, ret);
	}
#endif

	if (ret < PACKET_HEAD_SIZE) {
		if (ret >= 0) {
			DEBUG_LOG("usb_bulk_read() returned ret=%d < 8", ret);
		}
		tf->error = -1;
    }
	else {
		__u16 len = get_u16(&reply->length);

		if (ret < len) {
			DEBUG_LOG("usb_bulk_read() returned ret=%d < len=%d", ret, len);
			tf->error = -1;
		}
		else {
			print_packet(tf->tracefh, "<<", tf->trace_level, reply);

			/* Make it easier to read */
			tf->error = 0;
		}
	}
    return tf->error;
}

static int tf_wait_for_req_data(tf_handle *tf, tf_fw_data_t *fw_data)
{
	tf_packet_t reply;

	/* Wait for it to ask for the first block */
	int ret = tf_get_response(tf, &reply);

	/*printf("tf_wait_for_req_data() ret=%d reply.cmd=%s\n", ret, tf_command_name(reply.cmd));*/

	if (ret != 0) {
		return -1;
	}

	if (reply.cmd == TF_FW_REQ_DATA) {
		/* Success - send data */
		/* Extract len and offset into fw_data */
		fw_data->seq = reply.seq;
		fw_data->len = get_u16(reply.data);
		fw_data->offset = get_u24(reply.data + 2);
		DEBUG_LOG("REQ_DATA: seq=%02X len=%u, offset=%lu", fw_data->seq, fw_data->len, fw_data->offset);
		return 0;
	}
	if (reply.cmd == TF_FW_END) {
		/* Success - done */
		return 1;
	}

	/* Unexpected response */
	return -1;
}

int tf_fw_upload(tf_handle *tf, tf_fw_data_t *fw_data)
{
	int ret;
    tf_packet_t req;

	/* But in USB means we send cmd=1 (which should be 2) *before* we receive cmd=1 */
	tf_req_init(&req, TF_FW_PC_TO_STB, 0);
	tf_req_done(tf, &req);

    ret = tf_send(tf, &req);
	if (ret == 0) {
		tf_packet_t reply;

		ret = tf_get_response(tf, &reply);
		/*printf("Got ret=%d, reply.cmd=%d\n", ret, reply.cmd);*/
		if (ret != 0 || reply.cmd != TF_FW_ID) {
			return -1;
		}

		/* Now wait for it to ask for the first block */
		return tf_wait_for_req_data(tf, fw_data);
	}
	return -1;
}

int tf_fw_upload_next(tf_handle *tf, const void *buf, size_t len, tf_fw_data_t *fw_data)
{
	/* Send the data */
    tf_packet_t req;
	int ret;

	if (len > MAX_DATA_SIZE) {
		syslog(LOG_ERR, "Attempt to send too much data (%ld)", (long)len);
		return -1;
	}

	/* Use the same seq as the request */
	tf_req_init(&req, TF_FW_DATA, fw_data->seq);
	tf_req_putdata(&req, buf, len);
	tf_req_done(tf, &req);

    ret = tf_send(tf, &req);

	if (ret != 0) {
		return -1;
	}
	
	/* Now wait for it to ask for the next block */
	return tf_wait_for_req_data(tf, fw_data);
}

int tf_fw_reboot(tf_handle *tf)
{
    tf_packet_t req;

	tf_req_init(&req, TF_FW_REBOOT, 0);
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
		__u8 *p = (__u8 *) packet;
		/*__u8 *d = (__u8 *) packet->data;*/
		__u8 *d = p;
		/*int num = trace_level == 1 ? 64 : (len + 8 + 1);*/
		int num = 64;

		fprintf(fh, "(%ld) %s: cmd=%s(0x%02X), len=%d, seq=0x%02X\n", time(0), prefix, tf_command_name(packet->cmd), packet->cmd, len, packet->seq);
		fprintf(fh, "HEADER:");
		for (i = 0; i < 8; ++i) {
			fprintf(fh, " %02x", p[i]);
		}
		for (i = 0; i < num && i < ((len + 8 + 1 + 1) & ~1); ++i) {
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
static const char *tf_command_name(__u8 cmd)
{
	static char buf[64];

	switch (cmd) {
		case TF_FW_ID: return "ID";
		case TF_FW_PC_TO_STB: return "PC_TO_STB";
		case TF_FW_STB_TO_PC: return "STB_TO_PC";

		case TF_FW_REQ_DATA: return "REQ_DATA";
		case TF_FW_DATA: return "DATA";
		case TF_FW_END: return "END";
	}

	sprintf(buf, "UNKNOWN(%02X)", cmd);
	return buf;
}
