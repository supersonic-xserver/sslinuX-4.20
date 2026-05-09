// SPDX-License-Identifier: GPL-2.0
/*
 * lznt.c - NTFS3 LZNT compression support
 *
 * Copyright (c) 2020-2024 Paragon Software GmbH
 * Backport to sslinuX-4.20
 */

#include "ntfs3.h"

/*
 * ntfs3_lznt_init - Initialize LZNT decompressor
 */
int ntfs3_lznt_init(void)
{
	return 0;
}

/*
 * ntfs3_lznt_decompress - Decompress LZNT compressed data
 */
int ntfs3_lznt_decompress(const void *src, int src_len, void *dst, int dst_len)
{
	/* TODO: Implement LZNT decompression */
	return 0;
}

/*
 * ntfs3_lznt_compress - Compress data with LZNT
 */
int ntfs3_lznt_compress(const void *src, int src_len, void *dst, int dst_len)
{
	/* TODO: Implement LZNT compression */
	return 0;
}