// SPDX-License-Identifier: GPL-2.0
/*
 * bitfunc.c - NTFS3 bitmap operations
 *
 * Copyright (c) 2020-2024 Paragon Software GmbH
 * Backport to sslinuX-4.20
 */

#include "ntfs3.h"

/*
 * ntfs3_set_bit - Set bit in bitmap
 */
void ntfs3_set_bit(unsigned long *bitmap, int bit)
{
	set_bit(bit, bitmap);
}

/*
 * ntfs3_clear_bit - Clear bit in bitmap
 */
void ntfs3_clear_bit(unsigned long *bitmap, int bit)
{
	clear_bit(bit, bitmap);
}

/*
 * ntfs3_test_bit - Test bit in bitmap
 */
int ntfs3_test_bit(unsigned long *bitmap, int bit)
{
	return test_bit(bit, bitmap);
}

/*
 * ntfs3_find_next_zero - Find next zero bit
 */
int ntfs3_find_next_zero(unsigned long *bitmap, int start, int max)
{
	return find_next_zero_bit(bitmap, max, start);
}

/*
 * ntfs3_find_next - Find next set bit
 */
int ntfs3_find_next(unsigned long *bitmap, int start, int max)
{
	return find_next_bit(bitmap, max, start);
}