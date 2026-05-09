// SPDX-License-Identifier: GPL-2.0
/*
 * file.c - NTFS3 file operations
 *
 * Copyright (c) 2020-2024 Paragon Software GmbH
 *
 * Backport to sslinuX-4.20
 */

#include "ntfs3.h"
#include <linux/moduleparam.h>

/*
 * ntfs3_open - Open file
 */
static int ntfs3_open(struct inode *inode, struct file *filp)
{
	/* TODO: Implement file open */
	return 0;
}

/*
 * ntfs3_release - Release file
 */
static int ntfs3_release(struct inode *inode, struct file *filp)
{
	/* TODO: Implement file release */
	return 0;
}

/*
 * ntfs3_read_iter - Read from file
 */
static ssize_t ntfs3_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
	struct file *filp = iocb->ki_filp;
	struct inode *inode = file_inode(filp);
	ssize_t ret;

	inode_lock(inode);
	ret = generic_file_read_iter(iocb, to);
	inode_unlock(inode);

	return ret;
}

/*
 * ntfs3_write_iter - Write to file
 */
static ssize_t ntfs3_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct file *filp = iocb->ki_filp;
	struct inode *inode = file_inode(filp);
	ssize_t ret;

	inode_lock(inode);
	ret = generic_file_write_iter(iocb, from);
	inode_unlock(inode);

	return ret;
}

/*
 * ntfs3_fsync - Sync file to disk
 */
static int ntfs3_fsync(struct file *file, loff_t start, loff_t end,
		       int datasync)
{
	struct inode *inode = file_inode(file);
	int err;

	inode_lock(inode);
	err = file_write_and_wait_range(file, start, end);
	if (!err) {
		/* TODO: Update timestamps */
		inode->i_mtime = current_time(inode);
		mark_inode_dirty(inode);
	}
	inode_unlock(inode);

	return err;
}

/*
 * ntfs3_fallocate - Allocate file space
 *
 * For 5800X3D: Pre-allocate to reduce fragmentation (Dirty Frag fix)
 */
static long ntfs3_fallocate(struct file *file, int mode, loff_t offset,
			    loff_t len)
{
	struct inode *inode = file_inode(file);
	struct super_block *sb = inode->i_sb;
	loff_t new_size = offset + len;
	int ret;

	/* Don't allow fallocate on directories or unsupported modes */
	if (S_ISDIR(inode->i_mode))
		return -EISDIR;
	if (mode & ~(FALLOC_FL_KEEP_SIZE | FALLOC_FL_PUNCH_HOLE))
		return -EOPNOTSUPP;

	inode_lock(inode);

	/* Check for punch hole */
	if (mode & FALLOC_FL_PUNCH_HOLE) {
		/* TODO: Implement hole punching */
		ret = -EOPNOTSUPP;
		goto out;
	}

	/* Pre-allocate to prevent fragmentation */
	if (new_size > inode->i_size) {
		/* Extend file first */
		ret = inode_newsize_ok(inode, new_size);
		if (ret)
			goto out;

		/* TODO: Allocate clusters */
		/* For now, just update size */
		i_size_write(inode, new_size);
		mark_inode_dirty(inode);
	}

	ret = 0;

out:
	inode_unlock(inode);
	return ret;
}

/* File operations for ntfs3 */
const struct file_operations ntfs3_file_ops = {
	.llseek		= generic_file_llseek,
	.read_iter	= ntfs3_read_iter,
	.write_iter	= ntfs3_write_iter,
	.open		= ntfs3_open,
	.release	= ntfs3_release,
	.fsync		= ntfs3_fsync,
	.fallocate	= ntfs3_fallocate,
	.unlocked_ioctl	= NULL, /* TODO: Add ioctl support */
};

/* Directory operations for ntfs3 */
const struct file_operations ntfs3_dir_ops = {
	.iterate	= NULL, /* TODO: Implement directory iteration */
	.fsync		= ntfs3_fsync,
};

static int __init ntfs3_init(void)
{
	pr_info("NTFS3 file operations initialized\n");
	return 0;
}

static void __exit ntfs3_exit(void)
{
	pr_info("NTFS3 file operations unloaded\n");
}

module_init(ntfs3_init);
module_exit(ntfs3_exit);