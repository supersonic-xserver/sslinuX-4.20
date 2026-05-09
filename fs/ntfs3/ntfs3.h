/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ntfs3.h - NTFS3 (Paragon) driver header
 *
 * Copyright (c) 2020-2024 Paragon Software GmbH
 * 
 * Backport to Linux 4.20 for sslinuX-4.20
 * 
 * VFS abstraction layer for 4.20 compatibility:
 * - Folios -> Pages (folio_* -> page_*)
 * - iomap infrastructure shimming
 * - Inode ops compatibility
 */

#ifndef _LINUX_NTFS3_H
#define _LINUX_NTFS3_H

#include <linux/fs.h>
#include <linux/iomap.h>
#include <linux/buffer_head.h>
#include <linux/mpage.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/slab.h>
#include <linux/nls.h>
#include <linux/workqueue.h>
#include <linux/bit_spinlock.h>

/* Forward declarations */
struct ntfs_sb_info;
struct ntfs_inode;
struct ntfs_attr;
struct runs_tree;

/* Compatibility shims for 4.20 */
#ifndef HAVE_FOLIO
/* 4.20 doesn't have folios - use struct page directly */
#define folio战队(page)		(page)
#define folio_page(folio, idx)	((folio) + (idx))
#define folio_address(folio)	page_address(folio)
#define folio_lock(folio)	lock_page(folio)
#define folio_unlock(folio)	unlock_page(folio)
#define folio_mark_dirty(folio)	__set_page_dirty_nobuffers(folio)
#define folio_test_uptodate(folio) PageUptodate(folio)
#define folio_set_uptodate(folio)	SetPageUptodate(folio)
#define folio_clear_uptodate(folio)	ClearPageUptodate(folio)
#define folio_nr_pages(folio)	1
#define folio_idx(folio)	0
#endif

/* iomap shims for 4.20 */
struct ntfs3_iomap {
	struct iomap iomap;
	u32 valid;
};

static inline int ntfs3_iomap_begin(struct inode *inode, loff_t pos,
				    loff_t length, unsigned flags,
				    struct iomap *iomap)
{
	/* 4.20 iomap - simplified wrapper */
	memset(iomap, 0, sizeof(*iomap));
	iomap->type = IOMAP_HOLE;
	return 0;
}

/* Page operations for 4.20 */
static inline struct page *ntfs3_grab_page(struct address_space *mapping,
					   pgoff_t index)
{
	return grab_cache_page(mapping, index);
}

static inline void ntfs3_flush_dcache_pages(struct page **pages,
					     unsigned int nr_pages)
{
	flush_dcache_pages(pages, nr_pages);
}

/* Attribute operations */
enum {
	NTFS_ATTR_STANDARD = 0,
	NTFS_ATTR_LIST = 1,
	NTFS_ATTR_FILE_NAME = 2,
	NTFS_ATTR_VOLUME_NAME = 3,
	NTFS_ATTR_VOLUME_INFORMATION = 4,
	NTFS_ATTR_DATA = 5,
	NTFS_ATTR_INDEX_ROOT = 6,
	NTFS_ATTR_INDEX_ALLOCATION = 7,
	NTFS_ATTR_BITMAP = 8,
	NTFS_ATTR_REPARSE_POINT = 9,
	NTFS_ATTR_EA = 10,
	NTFS_ATTR_EA_INFORMATION = 11,
	NTFS_ATTR_LOGGING_UTILITY = 12,
	NTFS_ATTR_UNIX = 13,
	NTFS_ATTR_PROPERTY = 14,
	NTFS_ATTR_SID = 15,
	NTFS_ATTR_SECURITY = 16,
	NTFS_ATTR_ELI = 17,
};

/* Maximum attribute size */
#define NTFS_MAX_ATTRIBUTE_SIZE	(16 * 1024 * 1024 * 1024ULL)

/* Cluster sizes */
#define NTFS_CLUSTER_SIZE_MIN	512
#define NTFS_CLUSTER_SIZE_MAX	(16 * 1024 * 1024)

/* Runlist */
struct ntfs_run {
	s64 lcn;	/* Logical cluster number (-1 for sparse) */
	s64 len;	/* Length in clusters */
};

struct ntfs_ubuf {
	u64 vcn;
	u64 lcn;
	u64 len;
};

/* Attribute search context */
struct ntfs_search_context {
	struct ATTR_FILE_NAME *fname;
	struct ATTR_LIST_ENTRY *entry;
	void *attr;
};

/* Inode flags */
#define NTFS_EFLAG_CHANGED	0x01
#define NTFS_EFLAG_NEW		0x02
#define NTFS_EFLAG_IN_ROOT	0x04

/* Superblock flags */
#define NTFS_FLAG_DIRTY		0x01
#define NTFS_FLAG_HDRS_ONLY	0x02
#define NTFS_FMT_NTFS		0x03
#define NTFS_MFTMIRR_LCN	0x00

/* Compression */
#define NTFS_LZNT_CUNIT		0x0C

/* Version info */
#define NTFS_V0_1_VERSION	((0 << 16) | (1))
#define NTFS_V3_VERSION		((3 << 16) | (0))

/* MFT record numbers */
#define MFT_REC_MFT		0
#define MFT_REC_MFT_MIRR	1
#define MFT_REC_LOG_FILE	2
#define MFT_REC_VOLUME		3
#define MFT_REC_ROOT		4
#define MFT_REC_BITMAP		5
#define MFT_REC_BOOT		6
#define MFT_REC_BAD_CLUST	7
#define MFT_REC_QUOTA		8
#define MFT_REC_SECURE		9
#define MFT_REC_UPCASE		10
#define MFT_REC_EXTEND		11
#define MFT_REC_RESERVED	12
#define MFT_REC_FREE		16

/* Boot sector constants */
#define NTFS_BOOT_SECTOR_SIZE	512
#define NTFS_LABEL_MAX		64

/* Cluster bitmap attributes */
#define SPARSE_BITMAP_LCN	(~0ULL)
#define COMPRESSED_BITMAP_LCN	(~1ULL)

/* Volume flags */
#define VOLUME_FLAG_DIRTY	0x0001
#define VOLUME_FLAG_RESIZE_LOGFILE	0x0002

/* NTFS file attributes */
#define NTFS_ATTR_ARCHIVE	(1 << 0)
#define NTFS_ATTR_COMPRESSED	(1 << 1)
#define NTFS_ATTREncrypted	(1 << 2)
#define NTFS_ATTR_HIDDEN	(1 << 3)
#define NTFS_ATTR_READONLY	(1 << 4)
#define NTFS_ATTR_SYSTEM	(1 << 5)
#define NTFS_ATTR_TEMPORARY	(1 << 6)

/* Standard information attributes */
struct STANDARD_INFORMATION {
	__le64 crtime;
	__le64 mtime;
	__le64 ctime;
	__le64 atime;
	__le32 attributes;
	__le32 pad;
};

/* File name attributes */
struct ATTR_FILE_NAME {
	__le64 parent_ref;
	__le64 mtime;
	__le64 ctime;
	__le64 atime;
	__le64 all_size;
	struct {
		__le16 size;
		__le16 res;
		__le16 name[0];
	} fn;
};

#define FILE_NAME_LEN(f) (((f)->fn.size - offsetof(struct ATTR_FILE_NAME, fn.name)) / 2)

/* Volume information attributes */
struct ATTR_VOLUME_INFORMATION {
	__u8 major_ver;
	__u8 minor_ver;
	__le16 flags;
	__le32 pad;
};

/* Index header */
struct INDEX_HDR {
	__le32 entries_offset;
	__le32 index_length;
	__le32 allocated_size;
	__le32 flags;
};

#define INDEX_HDR_FIRST_ENTRY	0x00
#define INDEX_HDR_NODE_MASK	0x01

/* Index entry flags */
#define INDEX_ENTRY_NODE		0x01
#define INDEX_ENTRY_END		0x02

/* MFT record header */
struct MFT_RECORD {
	__le32 magic;
	__le16 fixup_offset;
	__le16 fixup_size;
	__le64 lsn;
	__le16 sequence;
	__le16 link_count;
	__le16 attrs_offset;
	__le16 flags;
	__le32 bytes_in_use;
	__le32 bytes_allocated;
	__le64 base_mft_record;
	__le16 next_attr_id;
	__le32 reserved;
	__le32 mft_record_index;
};

/* Attribute list entry */
struct ATTR_LIST_ENTRY {
	__le64 type;
	__le16 size;
	__u8 name_offset;
	__u8 name_size;
	__le16 flags;
	__le64 owner_ref;
	__le16 instance;
	__le16 reserved;
	__le64 first_vcn;
	__le16 name[0];
};

/* Attribute header (non-resident) */
struct ATTR {
	__le32 type;
	__le32 size;
	__u8 resident;
	__u8 name_offset;
	__le16 name_size;
	__le16 flags;
	__le16 instance;
	union {
		struct {
			__le32 value_size;
			__le16 value_offset;
			__u8 flags;
			__u8 reserved;
		} resident;
		struct {
			__le64 alloc_size;
			__le64 data_size;
			__le64 valid_size;
			__le64 data_offset;
			__le16 compression_unit;
			__le16 reserved;
			__le64 CompressionUnitSize;
		} non_resident;
	};
};

/* Attribute types */
enum ATTR_TYPE {
	AT_UNSPECIFIED = 0,
	AT_STANDARD_INFORMATION = 0x10,
	AT_ATTRIBUTE_LIST = 0x20,
	AT_FILE_NAME = 0x30,
	AT_OBJECT_ID = 0x40,
	AT_SECURITY_DESCRIPTOR = 0x50,
	AT_VOLUME_NAME = 0x60,
	AT_VOLUME_INFORMATION = 0x70,
	AT_DATA = 0x80,
	AT_INDEX_ROOT = 0x90,
	AT_INDEX_ALLOCATION = 0xA0,
	AT_BITMAP = 0xB0,
	AT_REPARSE_POINT = 0xC0,
	AT_EA_INFORMATION = 0xD0,
	AT_EA = 0xE0,
	AT_LOGGED_UTILITY_STREAM = 0x100,
};

/* Attribute flags */
#define ATTR_FLAG_COMPRESSED	0x0001
#define ATTR_FLAG_ENCRYPTED	0x4000
#define ATTR_FLAG_SPARSE	0x8000

/* Boot file reference */
struct NTFS_BOOT_SECTOR {
	__u8 jump[3];
	__u8 oem[8];
	__le16 bytes_per_sector;
	__u8 sectors_per_cluster;
	__u8 reserved_sectors[7];
	__u8 media_type;
	__u8 reserved1[2];
	__le16 sectors_per_track;
	__le16 heads;
	__le32 hidden_sectors;
	__u8 reserved2[8];
	__le64 total_sectors;
	__le64 mft_cluster_location;
	__le64 mft_mirror_cluster_location;
	__le32 clusters_per_mft_record;
	__le32 clusters_per_index_record;
	__le64 volume_serial;
	__le32 checksum;
	__u8 boot_sector_code[426];
	__le16 magic;
};

#define NTFS_SB_MAGIC 0xAA55

/* Helper functions */
static inline bool is_mft_rec(int mref)
{
	return mref < MFT_REC_RESERVED;
}

static inline bool is_bitmap_ino(int ino)
{
	return ino == MFT_REC_BITMAP;
}

/* Cluster operations */
static inline unsigned long clusters_to_bytes(struct ntfs_sb_info *sbi, s64 clusters)
{
	return clusters << sbi->cluster_bits;
}

static inline s64 bytes_to_clusters(struct ntfs_sb_info *sbi, unsigned long bytes)
{
	return bytes >> sbi->cluster_bits;
}

static inline unsigned long bytes_to_cluster_size(struct ntfs_sb_info *sbi)
{
	return 1UL << sbi->cluster_bits;
}

/* ntfs3 compatibility header for VFS */
#define ntfs3_get_block		mpage_get_block
#define ntfs3_readpage		mpage_readpage

#endif /* _LINUX_NTFS3_H */