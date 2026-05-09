// SPDX-License-Identifier: GPL-2.0
/*
 * inode.c - NTFS3 inode operations
 *
 * Copyright (c) 2020-2024 Paragon Software GmbH
 *
 * Backport to sslinuX-4.20
 * VFS inode ops backport from 5.15+
 */

#include "ntfs3.h"
#include <linux/pagemap.h>
#include <linux/swap.h>

/* 5800X3D L3 optimization: keep more pages in cache */
#define NTFS3_L3_CACHE_PAGES	8192

/* Inode private data */
struct ntfs_inode_info {
	struct inode vfs_inode;
	struct ntfs3_sb_info *sbi;
	u64 mft_no;
	struct mutex mrec_lock;
	
	/* Cached pages for L3 optimization */
	struct list_head page_list;
	int page_count;
	
	/* File attributes */
	unsigned long flags;
	
	/* Times */
	struct timespec i_crtime;
	struct timespec i_mtime;
	struct timespec i_atime;
	
	/* Extent information */
	u8 valid;
	u8 moderate;
	
	/* Attribute list */
	struct ATTR_LIST_ENTRY *attr_list;
	u32 attr_list_size;
};

/* Convert between VFS inode and ntfs_inode_info */
static inline struct ntfs_inode_info *ntfs_i(struct inode *inode)
{
	return container_of(inode, struct ntfs_inode_info, vfs_inode);
}

/*
 * ntfs3_readpage - Read page from NTFS volume
 *
 * Uses mpage_readpage for 4.20 compatibility
 */
static int ntfs3_readpage(struct file *file, struct page *page)
{
	return mpage_readpage(page, ntfs3_get_block);
}

/*
 * ntfs3_writepage - Write page to NTFS volume
 */
static int ntfs3_writepage(struct page *page, struct writeback_control *wbc)
{
	/* TODO: Implement proper writepage with journaling */
	block_write_full_page(page, ntfs3_get_block, wbc);
	return 0;
}

/*
 * ntfs3_writepages - Write multiple pages
 */
static int ntfs3_writepages(struct address_space *mapping,
			   struct writeback_control *wbc)
{
	return mpage_writepages(mapping, wbc, ntfs3_get_block);
}

/*
 * ntfs3_setattr - Set inode attributes
 */
int ntfs3_setattr(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = d_inode(dentry);
	struct ntfs_inode_info *ni = ntfs_i(inode);
	int error;

	/* Check mode */
	if (attr->ia_valid & ATTR_MODE) {
		/* Handle mode changes */
		if (attr->ia_mode & S_ISUID)
			attr->ia_mode &= ~S_ISUID;
		if (attr->ia_mode & (S_ISGID | S_IXGRP))
			attr->ia_mode &= ~S_ISGID;
	}

	/* Set times */
	if (attr->ia_valid & ATTR_ATIME) {
		inode->i_atime = attr->ia_atime;
		ni->i_atime = attr->ia_atime;
	}
	if (attr->ia_valid & ATTR_MTIME) {
		inode->i_mtime = attr->ia_mtime;
		ni->i_mtime = attr->ia_mtime;
	}
	if (attr->ia_valid & ATTR_CTIME) {
		inode->i_ctime = attr->ia_ctime;
		ni->i_crtime = attr->ia_ctime;
	}

	/* Mark inode dirty */
	mark_inode_dirty(inode);

	return 0;
}

/*
 * ntfs3_getattr - Get inode attributes
 */
int ntfs3_getattr(const struct path *path, struct kstat *stat,
		  u32 request_mask, unsigned int flags)
{
	struct inode *inode = path->dentry->d_inode;
	struct ntfs_inode_info *ni = ntfs_i(inode);

	stat->ino = ni->mft_no;
	stat->mode = inode->i_mode;
	stat->nlink = inode->i_nlink;
	stat->uid = inode->i_uid.val;
	stat->gid = inode->i_gid.val;
	stat->size = i_size_read(inode);
	stat->atime = inode->i_atime;
	stat->mtime = inode->i_mtime;
	stat->ctime = inode->i_ctime;
	stat->blksize = 4096;
	stat->blocks = inode->i_blocks;

	return 0;
}

/*
 * ntfs3_alloc_pages - Allocate pages with L3 cache awareness
 *
 * For 5800X3D: Pre-allocate to reduce fragmentation
 */
int ntfs3_alloc_pages(struct ntfs_inode_info *ni, unsigned int pages_needed)
{
	struct page **pages;
	int i;

	pages = kmalloc_array(pages_needed, sizeof(struct page *), GFP_NOFS);
	if (!pages)
		return -ENOMEM;

	/* Pre-allocate pages */
	for (i = 0; i < pages_needed; i++) {
		pages[i] = alloc_page(GFP_NOFS);
		if (!pages[i]) {
			while (--i >= 0)
				put_page(pages[i]);
			kfree(pages);
			return -ENOMEM;
		}
		/* Add to LRU for 5800X3D L3 optimization */
		lru_cache_add(pages[i]);
		list_add(&pages[i]->lru, &ni->page_list);
	}

	kfree(pages);
	ni->page_count += pages_needed;

	return 0;
}

/*
 * ntfs3_truncate - Truncate inode to new size
 */
void ntfs3_truncate(struct inode *inode)
{
	struct ntfs_inode_info *ni = ntfs_i(inode);

	if (!S_ISREG(inode->i_mode))
		return;

	/* Drop pages beyond new size */
	truncate_inode_pages(inode->i_mapping, inode->i_size);

	/* Mark inode dirty */
	mark_inode_dirty(inode);
}

/*
 * ntfs3_evict_inode - Evict inode from cache
 */
void ntfs3_evict_inode(struct inode *inode)
{
	struct ntfs_inode_info *ni = ntfs_i(inode);
	struct page *page, *tmp;

	/* Flush any dirty pages */
	if (inode->i_nlink) {
		/* TODO: Write inode to MFT */
	}

	/* Free cached pages */
	list_for_each_entry_safe(page, tmp, &ni->page_list, lru) {
		list_del(&page->lru);
		put_page(page);
	}
	ni->page_count = 0;

	/* Free attribute list */
	kfree(ni->attr_list);
	ni->attr_list = NULL;

	/* Clear inode */
	clear_inode(inode);
}

/*
 * ntfs3_sync_inode - Sync inode to disk
 */
int ntfs3_sync_inode(struct inode *inode)
{
	/* TODO: Implement proper inode sync */
	mark_inode_dirty(inode);
	return 0;
}

/*
 * ntfs3_update_parent - Update parent directory times
 */
void ntfs3_update_parent(struct inode *inode, struct inode *parent,
			  int mode)
{
	if (!parent)
		return;

	parent->i_mtime = parent->i_ctime = current_time(parent);
	mark_inode_dirty(parent);
}

/* Address space operations for 5800X3D optimization */
const struct address_space_operations ntfs3_aops = {
	.readpage	= ntfs3_readpage,
	.writepage	= ntfs3_writepage,
	.writepages	= ntfs3_writepages,
	.set_page_dirty	= __set_page_dirty_nobuffers,
};

/* Inode operations */
const struct inode_operations ntfs3_inode_ops = {
	.setattr	= ntfs3_setattr,
	.getattr	= ntfs3_getattr,
	.update_parent	= ntfs3_update_parent,
};

/* Regular file operations */
const struct file_operations ntfs3_file_ops = {
	.llseek		= generic_file_llseek,
	.read_iter	= generic_file_read_iter,
	.write_iter	= generic_file_write_iter,
	.mmap		= generic_file_mmap,
	.fsync		= noop_fsync,
};

/* Directory operations */
const struct file_operations ntfs3_dir_ops = {
	.iterate	= NULL, /* TODO: Implement iterate */
	.fsync		= noop_fsync,
};

/*
 * ntfs3_iget - Get inode by MFT number
 */
struct inode *ntfs3_iget(struct super_block *sb, unsigned long ino)
{
	struct ntfs3_sb_info *sbi = sb->s_fs_info;
	struct ntfs_inode_info *ni;
	struct inode *inode;

	/* Allocate new inode */
	inode = new_inode(sb);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	ni = ntfs_i(inode);
	ni->sbi = sbi;
	ni->mft_no = ino;

	/* Initialize lists */
	INIT_LIST_HEAD(&ni->page_list);
	ni->page_count = 0;
	ni->valid = 0;
	ni->moderate = 0;

	/* Set default mode based on MFT record */
	if (ino == MFT_REC_ROOT)
		inode->i_mode = S_IFDIR;
	else
		inode->i_mode = S_IFREG;

	inode_init_owner(inode, NULL, inode->i_mode);

	/* Set default times */
	inode->i_mtime = inode->i_atime = inode->i_ctime = current_time(inode);
	ni->i_crtime = ni->i_mtime;

	/* Set operations */
	inode->i_op = &ntfs3_inode_ops;
	inode->i_fop = S_ISDIR(inode->i_mode) ? &ntfs3_dir_ops : &ntfs3_file_ops;
	inode->i_mapping->a_ops = &ntfs3_aops;

	/* 5800X3D optimization: Increase readahead */
	inode->i_mapping->nr_pages = NTFS3_L3_CACHE_PAGES;

	return inode;
}

/*
 * ntfs3_read_inode - Read inode from MFT
 */
int ntfs3_read_inode(struct inode *inode)
{
	/* TODO: Implement proper MFT record reading */
	struct ntfs_inode_info *ni = ntfs_i(inode);

	/* Initialize with default values */
	inode->i_mode = S_IFREG;
	inode->i_size = 0;
	inode->i_blocks = 0;

	return 0;
}