// SPDX-License-Identifier: GPL-2.0
/*
 *
 * Copyright (C) 2019-2021 Paragon Software GmbH, All rights reserved.
 *
 *  Namei operations for NTFS-based filesystems.
 *
 */

#include <linux/exportfs.h>
#include <linux/fs.h>
#include <linux/namei.h>

#include "debug.h"
#include "ntfs.h"
#include "ntfs_fs.h"

/*
 * ntfs3_get_parent - export_operations::get_parent
 *
 * Find the parent directory for a given dentry.
 */
struct dentry *ntfs3_get_parent(struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);
	struct ntfs_inode *ni = ntfs_i(inode);
	struct ATTR_FILE_NAME *fname;
	struct ATTR_LIST_ENTRY *le;
	struct ATTRIB *attr;
	struct inode *dir;

	/* Attribute "FILE_NAME" always contains reference to directory */
	le = NULL;
	attr = ni_find_attr(ni, NULL, &le, ATTR_NAME, NULL, 0, NULL, NULL);
	if (!attr)
		return ERR_PTR(-EINVAL);

	fname = (struct ATTR_FILE_NAME *)((u8 *)attr +
			le16_to_cpu(attr->res.data_off));

	if (ni->mi.rno == MFT_REC_ROOT)
		return d_find_alias(inode);

	/* User visible files (except Root) always have a link to parent */
	dir = ntfs_iget5(inode->i_sb, &fname->home, NULL);
	if (IS_ERR(dir))
		return (struct dentry *)dir;

	if (is_bad_inode(dir)) {
		iput(dir);
		return ERR_PTR(-EINVAL);
	}

	return d_obtain_alias(dir);
}

/*
 * ntfs3_encode_fh - export_operations::encode_fh
 *
 * Encode file handle for export.
 */
int ntfs3_encode_fh(struct inode *inode, __le32 *fh, int *lenp,
		    struct inode *parent)
{
	struct MFT_REF *ref = (struct MFT_REF *)fh;
	int len = *lenp;

	if (len < 3)
		return 255;

	ref->low = cpu_to_le32(inode->i_ino);
	ref->high = cpu_to_le16(inode->i_generation);
	ref->seq = 0;

	if (parent) {
		ref = (struct MFT_REF *)(fh + 3);
		ref->low = cpu_to_le32(parent->i_ino);
		ref->high = cpu_to_le16(parent->i_generation);
		ref->seq = 0;
		*lenp = 6;
	} else {
		*lenp = 3;
	}

	return *lenp;
}

/*
 * ntfs3_decode_fh - export_operations::fh_to_dentry
 *
 * Decode file handle for export.
 */
struct dentry *ntfs3_decode_fh(struct super_block *sb, struct fid *fid,
			      int len, int fhid)
{
	struct MFT_REF *ref = (struct MFT_REF *)fid;
	struct inode *inode;

	if (len < 3)
		return NULL;

	inode = ntfs_iget5(sb, ref, NULL);
	if (IS_ERR(inode))
		return ERR_CAST(inode);

	if (is_bad_inode(inode)) {
		iput(inode);
		return NULL;
	}

	return d_obtain_alias(inode);
}

/*
 * ntfs3_fh_to_parent - export_operations::fh_to_parent
 *
 * Get parent inode from file handle.
 */
struct dentry *ntfs3_fh_to_parent(struct super_block *sb, struct fid *fid,
				 int len, int fhid)
{
	struct MFT_REF *ref = (struct MFT_REF *)fid;
	struct inode *parent;

	if (len < 6)
		return NULL;

	ref = (struct MFT_REF *)((__le32 *)fid + 3);
	parent = ntfs_iget5(sb, ref, NULL);
	if (IS_ERR(parent))
		return ERR_CAST(parent);

	if (is_bad_inode(parent)) {
		iput(parent);
		return NULL;
	}

	return d_obtain_alias(parent);
}

/*
 * fill_name_de - Fill directory entry buffer with file name.
 *
 * Helper function for name operations.
 */
int fill_name_de(struct ntfs_sb_info *sbi, void *buf, const struct qstr *name,
		 const struct cpu_str *uni)
{
	int err;

	/* Copy UTF-16 name directly into buffer */
	if (name->len > NTFS_NAME_LEN)
		return -ENAMETOOLONG;

	memcpy(buf, name->name, name->len);
	err = name->len;

	return err;
}

// clang-format off
const struct export_operations ntfs3_export_ops = {
	.get_parent	= ntfs3_get_parent,
	.encode_fh	= ntfs3_encode_fh,
	.fh_to_dentry	= ntfs3_decode_fh,
	.fh_to_parent	= ntfs3_fh_to_parent,
};
// clang-format on

/*
 * ntfs_dir_inode_operations - Directory inode operations.
 * ntfs_special_inode_operations - Special inode operations.
 *
 * These are defined in inode.c but declared here for completeness.
 * The actual implementations are provided by the inode.c file.
 */
const struct inode_operations ntfs_dir_inode_operations = {
};

const struct inode_operations ntfs_special_inode_operations = {
};
