// SPDX-License-Identifier: GPL-2.0
/*
 * super.c - NTFS3 superblock operations
 *
 * Copyright (c) 2020-2024 Paragon Software GmbH
 *
 * Backport to Linux 4.20 for sslinuX-4.20
 */

#include "ntfs3.h"
#include <linux/blkdev.h>
#include <linux/module.h>
#include <linux/parser.h>
#include <linux/seq_file.h>

/* Global ntfs3 options */
static char *ntfs3_dev_name;

/* Module parameters */
module_param(ntfs3_dev_name, charp, 0);
MODULE_PARM_DESC(ntfs3_dev_name, "NTFS3 device name");

/* Superblock private data */
struct ntfs3_sb_info {
	struct super_block *sb;
	struct buffer_head *boot_bh;
	struct NTFS_BOOT_SECTOR *boot;

	/* Volume info */
	u64 volume_size;
	u32 sector_size;
	u32 cluster_size;
	u32 cluster_bits;
	u64 mft_cluster;
	u64 mftmirr_cluster;
	u32 mft_record_size;
	u32 index_record_size;

	/* MFT */
	struct inode *mft_inode;
	struct address_space *mft_mapping;

	/* Upcase table */
	struct inode *upcase_inode;
	u16 *upcase;
	u32 upcase_len;

	/* Quota */
	struct inode *quota_inode;

	/* Bitmap */
	struct inode *bitmap_inode;
	unsigned long *bitmap;

	/* Flags */
	unsigned long flags;
#define NTFSFlags_DIRTY		0
#define NTFSFlags_LogReplay	1

	/* Encoding */
	struct nls_table *nls;

	/* Work queue */
	struct workqueue_struct *workqueue;
	struct work_struct work;
};

/* Global NTFS3 mount options */
enum {
	Opt_iocharset,
	Opt_errors,
	Opt_implicit,
	Opt_force,
	Opt_silent,
	Opt_help,
	Opt_show,
};

static const match_table_t options = {
	{Opt_iocharset, "iocharset=%s"},
	{Opt_errors, "errors=%s"},
	{Opt_implicit, "implicit"},
	{Opt_force, "force"},
	{Opt_silent, "silent"},
	{Opt_help, "help"},
	{Opt_show, "show"},
	{Opt_silent, "showopt"},
	{0, NULL}
};

/*
 * ntfs3_read_boot_sector - Read and validate NTFS boot sector
 */
static int ntfs3_read_boot_sector(struct super_block *sb)
{
	struct ntfs3_sb_info *sbi = sb->s_fs_info;
	struct buffer_head *bh;
	struct NTFS_BOOT_SECTOR *br;
	int err;

	bh = sb_bread(sb, 0);
	if (!bh) {
		ntfs_err(sb, "Failed to read boot sector");
		return -EIO;
	}

	sbi->boot_bh = bh;
	br = (struct NTFS_BOOT_SECTOR *)bh->b_data;

	/* Validate magic */
	if (le16_to_cpup(&br->magic) != NTFS_SB_MAGIC) {
		ntfs_err(sb, "Not an NTFS boot sector (magic=%x)",
			 le16_to_cpup(&br->magic));
		err = -EINVAL;
		goto out;
	}

	sbi->boot = br;

	/* Check bytes per sector */
	sbi->sector_size = le16_to_cpu(br->bytes_per_sector);
	if (!is_power_of_2(sbi->sector_size) ||
	    sbi->sector_size < 512 ||
	    sbi->sector_size > 4096) {
		ntfs_err(sb, "Invalid sector size %u", sbi->sector_size);
		err = -EINVAL;
		goto out;
	}

	/* Check sectors per cluster */
	if (!br->sectors_per_cluster ||
	    is_power_of_2(br->sectors_per_cluster)) {
		/* OK */
	} else {
		ntfs_err(sb, "Invalid sectors per cluster %u",
			 br->sectors_per_cluster);
		err = -EINVAL;
		goto out;
	}

	sbi->cluster_size = sbi->sector_size * br->sectors_per_cluster;
	sbi->cluster_bits = blksize_bits(sbi->cluster_size);

	/* MFT location */
	sbi->mft_cluster = le64_to_cpu(br->mft_cluster_location);
	sbi->mftmirr_cluster = le64_to_cpu(br->mft_mirror_cluster_location);

	/* MFT record size */
	sbi->mft_record_size = le32_to_cpu(br->clusters_per_mft_record);
	if (sbi->mft_record_size < 0)
		sbi->mft_record_size = 1 << (-sbi->mft_record_size);
	else
		sbi->mft_record_size <<= sbi->cluster_bits;

	/* Index record size */
	sbi->index_record_size = le32_to_cpu(br->clusters_per_index_record);
	if (sbi->index_record_size < 0)
		sbi->index_record_size = 1 << (-sbi->index_record_size);
	else
		sbi->index_record_size <<= sbi->cluster_bits;

	/* Volume size */
	sbi->volume_size = le64_to_cpu(br->total_sectors);
	sbi->volume_size <<= blksize_bits(sbi->sector_size);

	err = 0;

out:
	return err;
}

/*
 * ntfs3_put_super - Free superblock resources
 */
static void ntfs3_put_super(struct super_block *sb)
{
	struct ntfs3_sb_info *sbi = sb->s_fs_info;

	if (!sbi)
		return;

	/* Flush bitmap */
	if (sbi->bitmap_inode && sbi->bitmap)
		mark_inode_dirty(sbi->bitmap_inode);

	/* Destroy workqueue */
	if (sbi->workqueue)
		destroy_workqueue(sbi->workqueue);

	/* Free upcase table */
	kfree(sbi->upcase);

	/* Free bitmap */
	kfree(sbi->bitmap);

	/* Release boot sector buffer */
	brelse(sbi->boot_bh);

	/* Free superblock info */
	kfree(sbi);
	sb->s_fs_info = NULL;
}

/*
 * ntfs3_statfs - Get filesystem statistics
 */
static int ntfs3_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	struct super_block *sb = dentry->d_sb;
	struct ntfs3_sb_info *sbi = sb->s_fs_info;

	buf->f_type = NTFS_SB_MAGIC;
	buf->f_bsize = sbi->cluster_size;
	buf->f_blocks = sbi->volume_size >> sbi->cluster_bits;

	/* TODO: Calculate free clusters */
	buf->f_bfree = 0;
	buf->f_bavail = 0;

	buf->f_files = sbi->volume_size >> sbi->cluster_bits;
	buf->f_ffree = 0;

	buf->f_namelen = 255;

	return 0;
}

/*
 * ntfs3_sync_fs - Sync filesystem
 */
static int ntfs3_sync_fs(struct super_block *sb, int wait)
{
	/* TODO: Implement proper sync */
	return 0;
}

/*
 * ntfs3_write_inode - Write inode to disk
 */
static int ntfs3_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	/* TODO: Implement inode writing */
	return 0;
}

/*
 * ntfs3_remount - Handle filesystem remount
 */
static int ntfs3_remount(struct super_block *sb, int *flags, char *data)
{
	/* TODO: Implement remount */
	return 0;
}

/*
 * ntfs3_show_options - Show mount options
 */
static int ntfs3_show_options(struct seq_file *m, struct dentry *root)
{
	struct super_block *sb = root->d_sb;
	struct ntfs3_sb_info *sbi = sb->s_fs_info;

	if (sbi->nls)
		seq_printf(m, ",iocharset=%s", sbi->nls->charset);

	return 0;
}

/*
 * ntfs3_init_inode - Initialize inode cache
 */
static struct inode *ntfs3_alloc_inode(struct super_block *sb)
{
	struct inode *inode;

	inode = new_inode(sb);
	if (!inode)
		return NULL;

	return inode;
}

/*
 * ntfs3_destroy_inode - Destroy inode
 */
static void ntfs3_destroy_inode(struct inode *inode)
{
	if (inode)
		return;
}

/*
 * ntfs3_sync_fs_worker - Background sync worker
 */
static void ntfs3_sync_fs_worker(struct work_struct *work)
{
	struct ntfs3_sb_info *sbi = container_of(work, struct ntfs3_sb_info, work);

	/* TODO: Implement background sync */
}

/*
 * ntfs3_fill_super - Fill superblock
 */
static int ntfs3_fill_super(struct super_block *sb, void *data, int silent)
{
	struct ntfs3_sb_info *sbi;
	struct inode *root_inode;
	int err;

	/* Allocate superblock info */
	sbi = kzalloc(sizeof(struct ntfs3_sb_info), GFP_NOFS);
	if (!sbi)
		return -ENOMEM;

	sb->s_fs_info = sbi;
	sbi->sb = sb;

	/* Set default encoding */
	sbi->nls = load_nls("utf8");
	if (!sbi->nls)
		sbi->nls = load_nls_default();

	/* Read boot sector */
	err = ntfs3_read_boot_sector(sb);
	if (err) {
		if (!silent)
			ntfs_err(sb, "Failed to read boot sector: %d", err);
		goto out;
	}

	/* Create workqueue */
	sbi->workqueue = create_singlethread_workqueue("ntfs3");
	if (!sbi->workqueue) {
		err = -ENOMEM;
		goto out;
	}

	INIT_WORK(&sbi->work, ntfs3_sync_fs_worker);

	/* Allocate root inode */
	root_inode = ntfs3_alloc_inode(sb);
	if (!root_inode) {
		err = -ENOMEM;
		goto out;
	}

	inode_init_owner(root_inode, NULL, S_IFDIR);

	root_inode->i_mtime = root_inode->i_ctime = root_inode->i_atime =
		current_time(root_inode);

	sb->s_root = d_make_root(root_inode);
	if (!sb->s_root) {
		err = -ENOMEM;
		goto out;
	}

	/* Set dirty flag */
	sbi->flags |= 1 << NTFSFlags_DIRTY;

	return 0;

out:
	ntfs3_put_super(sb);
	return err;
}

/*
 * ntfs3_mount - Mount NTFS3 filesystem
 */
static struct dentry *ntfs3_mount(struct file_system_type *fs_type, int flags,
				  const char *dev_name, void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, ntfs3_fill_super);
}

/*
 * ntfs3_kill_sb - Kill superblock
 */
static void ntfs3_kill_sb(struct super_block *sb)
{
	/* Emergency shutdown */
	kill_block_super(sb);
}

/* NTFS3 filesystem type */
static struct file_system_type ntfs3_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "ntfs3",
	.mount		= ntfs3_mount,
	.kill_sb	= ntfs3_kill_sb,
	.fs_flags	= FS_REQUIRES_DEV,
};

static int __init ntfs3_init(void)
{
	int err;

	pr_info("NTFS3 driver loaded\n");
	err = register_filesystem(&ntfs3_fs_type);
	if (err)
		return err;

	return 0;
}

static void __exit ntfs3_exit(void)
{
	unregister_filesystem(&ntfs3_fs_type);
	pr_info("NTFS3 driver unloaded\n");
}

module_init(ntfs3_init);
module_exit(ntfs3_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("NTFS3 (Paragon) driver for Linux 4.20");
MODULE_AUTHOR("sslinuX-4.20");