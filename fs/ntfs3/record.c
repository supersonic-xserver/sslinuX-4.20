// SPDX-License-Identifier: GPL-2.0
/*
 * record.c - NTFS3 MFT record operations
 *
 * Copyright (c) 2020-2024 Paragon Software GmbH
 * Backport to sslinuX-4.20
 */

#include "ntfs3.h"

/*
 * ntfs3_read_mft_record - Read MFT record
 */
int ntfs3_read_mft_record(struct ntfs3_sb_info *sbi, int mref, struct MFT_RECORD **rec)
{
	/* TODO: Implement MFT record reading */
	return 0;
}

/*
 * ntfs3_write_mft_record - Write MFT record
 */
int ntfs3_write_mft_record(struct ntfs3_sb_info *sbi, int mref, struct MFT_RECORD *rec)
{
	/* TODO: Implement MFT record writing */
	return 0;
}

/*
 * ntfs3_init_mft_record - Initialize MFT record
 */
void ntfs3_init_mft_record(struct MFT_RECORD *rec, int mref)
{
	memset(rec, 0, sizeof(*rec));
	rec->magic = 0x454C4946; /* 'FILE' */
	rec->fixup_offset = 0x30;
	rec->sequence = 1;
}

/*
 * ntfs3_apply_fixup - Apply fixup values
 */
int ntfs3_apply_fixup(struct MFT_RECORD *rec, __le16 *fixup, int count)
{
	/* TODO: Implement fixup application */
	return 0;
}

/*
 * ntfs3_get_fixup - Get fixup values
 */
int ntfs3_get_fixup(struct MFT_RECORD *rec, __le16 **fixup, int *count)
{
	/* TODO: Implement fixup extraction */
	return 0;
}