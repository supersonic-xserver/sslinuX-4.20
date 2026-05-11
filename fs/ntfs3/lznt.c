// SPDX-License-Identifier: GPL-2.0
/*
 *
 * Copyright (C) 2019-2021 Paragon Software GmbH, All rights reserved.
 *
 *  LZNT compression/decompression for NTFS.
 *
 */

#include <linux/slab.h>
#include <linux/string.h>

#include "debug.h"
#include "ntfs.h"
#include "ntfs_fs.h"

/*
 * LZNT is a variation of LZ77 with a maximum match length of 4096.
 * The algorithm uses a simplified sliding window with variable-length
 * offset encoding.
 */

/* LZNT constants */
#define LZNT_MAX_MATCH_LEN 4096
#define LZNT_MIN_MATCH_LEN 3
#define LZNT_HASH_SIZE 0x1000

/* Hash table entry */
struct lznt_hash {
	u16 offset;
	u16 length;
};

/* LZNT context for compression */
struct lznt {
	struct lznt_hash hash[LZNT_HASH_SIZE];
};

/*
 * get_lznt_ctx - Get or create compression context.
 *
 * For simplicity, we don't actually use the level parameter
 * since LZNT is a fixed algorithm.
 */
struct lznt *get_lznt_ctx(int level)
{
	struct lznt *ctx;

	ctx = kzalloc(sizeof(struct lznt), GFP_NOFS);
	return ctx;
}

/*
 * decompress_lznt - Decompress LZNT compressed data.
 *
 * @compressed: Input buffer with compressed data.
 * @compressed_size: Size of compressed data.
 * @uncompressed: Output buffer for decompressed data.
 * @uncompressed_size: Size of uncompressed buffer.
 *
 * Returns: Number of bytes decompressed, or negative error.
 */
ssize_t decompress_lznt(const void *compressed, size_t compressed_size,
			void *uncompressed, size_t uncompressed_size)
{
	const u8 *src = compressed;
	u8 *dst = uncompressed;
	const u8 *src_end = src + compressed_size;
	u8 *dst_end = dst + uncompressed_size;
	u8 *dst_start = dst;

	while (src < src_end && dst < dst_end) {
		u8 flags = *src++;
		int i;

		/* Process 8 tokens per flag byte */
		for (i = 0; i < 8 && src < src_end && dst < dst_end; i++) {
			if (!(flags & (1 << i))) {
				/* Literal byte */
				*dst++ = *src++;
				continue;
			}

			/* Compressed match - read offset and length */
			{
				u16 offset_len;
				u16 offset;
				u16 length;
				const u8 *src_ptr;
				size_t j;

				if (src >= src_end)
					break;

				offset_len = src[0] | (src[1] << 8);
				src += 2;

				/* Offset is in bits 12-15, length in bits 8-11 minus 1 */
				offset = (offset_len >> 12) + 1;
				length = ((offset_len >> 8) & 0xF) + 3;

				if (offset > (dst - dst_start))
					offset = (u16)(dst - dst_start);

				if (length > (u16)(dst_end - dst))
					length = (u16)(dst_end - dst);

				/* Copy match */
				src_ptr = dst - offset;
				for (j = 0; j < length; j++)
					*dst++ = *src_ptr++;
			}
		}
	}

	return dst - dst_start;
}

/*
 * compress_lznt - Compress data using LZNT algorithm.
 *
 * @uncompressed: Input data to compress.
 * @uncompressed_size: Size of input data.
 * @compressed: Output buffer for compressed data.
 * @compressed_size: Size of output buffer.
 * @ctx: Compression context.
 *
 * Returns: Number of bytes written to compressed buffer.
 */
size_t compress_lznt(const void *uncompressed, size_t uncompressed_size,
		     void *compressed, size_t compressed_size,
		     struct lznt *ctx)
{
	const u8 *src = uncompressed;
	const u8 *src_end = src + uncompressed_size;
	u8 *dst = compressed;
	u8 *dst_end = dst + compressed_size;
	const u8 *match_start;
	size_t match_len;
	u8 flags = 0;
	int bit_pos = 0;

	/* Initialize hash table */
	if (ctx)
		memset(ctx->hash, 0, sizeof(ctx->hash));

	while (src < src_end) {
		u16 offset;
		size_t length;

		match_start = NULL;
		match_len = 0;

		/* Search for longest match in sliding window */
		{
			size_t search_start;
			const u8 *p;
			const u8 *q;
			size_t max_match;

			if (src - (u8 *)uncompressed > 4096)
				search_start = 4096;
			else
				search_start = src - (u8 *)uncompressed;

			max_match = LZNT_MAX_MATCH_LEN;
			if (src_end - src < max_match)
				max_match = src_end - src;

			/* Simple linear search for matches */
			for (p = src - search_start; p < src; p++) {
				size_t len = 0;

				q = p;
				while (q < src && src + len < src_end &&
				       *q == src[len] && len < max_match)
					len++;

				if (len > match_len && len >= LZNT_MIN_MATCH_LEN) {
					match_start = p;
					match_len = len;
				}
			}
		}

		if (match_len >= LZNT_MIN_MATCH_LEN) {
			/* Emit compressed token */
			offset = src - match_start;
			length = match_len - 3;

			/* Encode offset and length */
			{
				u16 offset_len;

				if (dst + 3 > dst_end)
					break;

				offset_len = (((offset - 1) & 0xFFF) << 12) |
					    (((length - 1) & 0xF) << 8);

				dst[0] = offset_len & 0xFF;
				dst[1] = (offset_len >> 8) & 0xFF;
				dst[2] = 0; /* Padding for alignment */
				dst += 2;

				flags |= (1 << bit_pos);
			}

			src += match_len;
		} else {
			/* Emit literal byte */
			if (dst + 1 > dst_end)
				break;

			*dst++ = *src++;

			flags |= (1 << bit_pos);
		}

		bit_pos++;
		if (bit_pos == 8) {
			/* Write flag byte and reset */
			if (dst >= dst_end)
				break;

			/* Move flags to front */
			memmove(dst + 1, dst - bit_pos, bit_pos);
			*dst = flags;

			dst += bit_pos + 1;
			flags = 0;
			bit_pos = 0;
		}
	}

	/* Write remaining flag byte */
	if (bit_pos > 0) {
		if (dst < dst_end)
			*dst++ = flags;
	}

	return dst - (u8 *)compressed;
}
