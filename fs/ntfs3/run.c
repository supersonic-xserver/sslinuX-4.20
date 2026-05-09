// SPDX-License-Identifier: GPL-2.0
/*
 * run.c - NTFS3 runlist operations
 *
 * Copyright (c) 2020-2024 Paragon Software GmbH
 * Backport to sslinuX-4.20
 */

#include "ntfs3.h"

/*
 * run_is_empty - Check if run is empty
 */
bool run_is_empty(struct ntfs_run *run)
{
	return run->lcn == SPARSE_BITMAP_LCN && run->len == 0;
}

/*
 * run_add - Add run to runlist
 */
int run_add(struct ntfs_run *runs, int *count, int max, s64 lcn, s64 len)
{
	if (*count >= max)
		return -ENOSPC;
	
	runs[*count].lcn = lcn;
	runs[*count].len = len;
	(*count)++;
	
	return 0;
}

/*
 * run_next - Get next run
 */
struct ntfs_run *run_next(struct ntfs_run *run)
{
	return run + 1;
}

/*
 * run_unpack - Unpack run from packed format
 */
int run_unpack(s64 *lcn_out, s64 *len_out, const char *src, int src_len)
{
	/* TODO: Implement run unpacking */
	*lcn_out = 0;
	*len_out = 0;
	return 0;
}

/*
 * run_pack - Pack run to packed format
 */
int run_pack(const struct ntfs_run *run, char *dst, int dst_len)
{
	/* TODO: Implement run packing */
	return 0;
}