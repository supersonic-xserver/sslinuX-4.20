// SPDX-License-Identifier: GPL-2.0
/*
 *
 * Copyright (C) 2019-2021 Paragon Software GmbH, All rights reserved.
 *
 *  Attribute list handling functions for NTFS-based filesystems.
 *
 */

#include <linux/fs.h>
#include <linux/slab.h>

#include "debug.h"
#include "ntfs.h"
#include "ntfs_fs.h"

/*
 * al_destroy - Free attribute list.
 */
void al_destroy(struct ntfs_inode *ni)
{
	struct ntfs_sb_info *sbi = ni->mi.sbi;

	if (ni->attr_list.le) {
		run_deallocate(sbi, &ni->attr_list.run, true);
		ni->attr_list.le = NULL;
	}
	ni->attr_list.size = 0;
	ni->attr_list.dirty = false;
}

/*
 * al_enumerate - Enumerate attribute list entries.
 *
 * NOTE: If @le is NULL, then the first entry is returned.
 */
struct ATTR_LIST_ENTRY *al_enumerate(struct ntfs_inode *ni,
				     struct ATTR_LIST_ENTRY *le)
{
	struct ATTR_LIST_ENTRY *next;
	u16 size;

	if (!ni->attr_list.le)
		return NULL;

	if (!le) {
		/* Start enumeration. */
		return ni->attr_list.le;
	}

	next = Add2Ptr(le, le16_to_cpu(le->size));
	size = le16_to_cpu(le->size);

	if ((u8 *)next >= (u8 *)ni->attr_list.le + ni->attr_list.size ||
	    next->type == ATTR_END)
		return NULL;

	return next;
}

/*
 * al_find_ex - Find attribute in attribute list.
 *
 * NOTE: The search starts from @le (if @le is not NULL) or from the beginning.
 *       If @vcn is not NULL, the search starts from the entry where vcn >= @vcn.
 */
struct ATTR_LIST_ENTRY *al_find_ex(struct ntfs_inode *ni,
				   struct ATTR_LIST_ENTRY *le,
				   enum ATTR_TYPE type, const __le16 *name,
				   u8 name_len, const CLST *vcn)
{
	struct ATTR_LIST_ENTRY *first = NULL;

	if (!ni->attr_list.le)
		return NULL;

	if (!le) {
		le = ni->attr_list.le;
	} else {
		le = Add2Ptr(le, le16_to_cpu(le->size));
	}

	for (;; le = Add2Ptr(le, le16_to_cpu(le->size))) {
		if ((u8 *)le >= (u8 *)ni->attr_list.le + ni->attr_list.size)
			return NULL;

		if (le->type == ATTR_END)
			return NULL;

		if (type != le->type)
			continue;

		if (vcn) {
			if (le64_to_cpu(le->vcn) < *vcn)
				continue;
		}

		if (name_len != le->name_len)
			continue;

		if (name_len && memcmp(name, le_name(le), name_len * sizeof(__le16)))
			continue;

		/* Match found, check for duplicate */
		if (first)
			return first;

		first = le;
	}

	return NULL;
}

/*
 * al_find_le - Find attribute list entry for given attribute.
 *
 * NOTE: Searches for entry that describes the same attribute as @attr.
 */
struct ATTR_LIST_ENTRY *al_find_le(struct ntfs_inode *ni,
				   struct ATTR_LIST_ENTRY *le,
				   const struct ATTRIB *attr)
{
	return al_find_ex(ni, le, attr->type, attr_name(attr), attr->name_len, NULL);
}

/*
 * ntfs_load_attr_list - Load attribute list into memory.
 */
int ntfs_load_attr_list(struct ntfs_inode *ni, struct ATTRIB *attr)
{
	struct ntfs_sb_info *sbi = ni->mi.sbi;
	int err = 0;
	struct ATTR_LIST_ENTRY *le;
	u32 list_size;
	struct runs_tree *run = &ni->attr_list.run;

	if (ni->attr_list.le)
		return 0;

	list_size = le32_to_cpu(attr->nres.data_size);
	if (list_size < sizeof(struct ATTR_LIST_ENTRY))
		return -EINVAL;

	le = kmalloc(list_size, GFP_NOFS);
	if (!le)
		return -ENOMEM;

	err = attr_load_runs(attr, ni, run, NULL);
	if (err)
		goto out;

	err = ntfs_read_run_nb(sbi, run, 0, le, list_size, NULL);
	if (err)
		goto out;

	ni->attr_list.le = le;
	ni->attr_list.size = list_size;

out:
	if (err)
		kfree(le);
	return err;
}

/*
 * al_add_le - Add new attribute list entry.
 *
 * Insert a new entry into the list at the correct position.
 */
int al_add_le(struct ntfs_inode *ni, enum ATTR_TYPE type, const __le16 *name,
	      u8 name_len, CLST svcn, __le16 id, const struct MFT_REF *ref,
	      struct ATTR_LIST_ENTRY **new_le)
{
	struct ntfs_sb_info *sbi = ni->mi.sbi;
	u16 size;
	struct ATTR_LIST_ENTRY *le;
	struct ATTR_LIST_ENTRY *prev;

	if (!ni->attr_list.le)
		return -EINVAL;

	/* Calculate entry size */
	size = le_size(name_len);

	/* Check if we need to expand the list */
	if (ni->attr_list.size + size >
	    (sbi->record_size - sizeof(struct ATTR_LIST_ENTRY)) * 4) {
		return -EINVAL;
	}

	/* Find insertion point */
	prev = NULL;
	le = al_find_ex(ni, NULL, type, name, name_len, &svcn);
	if (le) {
		/* Entry already exists */
		*new_le = le;
		return 0;
	}

	/* Create new entry */
	le = kmalloc(size, GFP_NOFS);
	if (!le)
		return -ENOMEM;

	le->type = type;
	le->size = cpu_to_le16(size);
	le->name_len = name_len;
	le->name_off = offsetof(struct ATTR_LIST_ENTRY, name);
	le->vcn = cpu_to_le64(svcn);
	le->id = id;
	le->ref = *ref;
	if (name_len && name)
		memcpy((void *)le_name(le), name, name_len * sizeof(__le16));

	ni->attr_list.dirty = true;
	*new_le = le;

	return 0;
}

/*
 * al_remove_le - Remove attribute list entry.
 */
bool al_remove_le(struct ntfs_inode *ni, struct ATTR_LIST_ENTRY *le)
{
	u16 size;
	u32 off;

	if (!ni->attr_list.le || !le)
		return false;

	size = le16_to_cpu(le->size);
	off = PtrOffset(ni->attr_list.le, le);

	/* Check bounds */
	if (off + size > ni->attr_list.size)
		return false;

	/* Remove entry */
	memmove(le, Add2Ptr(le, size), ni->attr_list.size - off - size);
	ni->attr_list.size -= size;
	ni->attr_list.dirty = true;

	return true;
}

/*
 * al_delete_le - Delete entries for a given attribute/VCN.
 */
bool al_delete_le(struct ntfs_inode *ni, enum ATTR_TYPE type, CLST vcn,
		  const __le16 *name, size_t name_len,
		  const struct MFT_REF *ref)
{
	struct ATTR_LIST_ENTRY *le;
	bool ret = false;

	if (!ni->attr_list.le)
		return false;

	le = al_find_ex(ni, NULL, type, name, name_len, NULL);
	if (!le)
		return false;

	while (le && le->type != ATTR_END) {
		if (le->type != type)
			break;

		if (name_len && le->name_len != name_len)
			break;

		if (name_len && memcmp(name, le_name(le), name_len * sizeof(__le16)))
			break;

		if (ref && memcmp(ref, &le->ref, sizeof(*ref)))
			break;

		if (al_remove_le(ni, le))
			ret = true;

		le = al_enumerate(ni, NULL);
	}

	return ret;
}

/*
 * al_update - Update attribute list on disk.
 */
int al_update(struct ntfs_inode *ni, int sync)
{
	struct ntfs_sb_info *sbi = ni->mi.sbi;
	struct ATTRIB *attr;
	struct ATTR_LIST_ENTRY *le;
	struct mft_inode *mi;
	int err;

	if (!ni->attr_list.dirty)
		return 0;

	if (!ni->attr_list.le)
		return 0;

	/* Find attribute list attribute */
	le = NULL;
	attr = ni_find_attr(ni, NULL, &le, ATTR_LIST, NULL, 0, NULL, &mi);
	if (!attr)
		return -EINVAL;

	err = ntfs_sb_write_run(sbi, &ni->attr_list.run, 0,
				ni->attr_list.le, ni->attr_list.size, sync);
	if (err)
		return err;

	ni->attr_list.dirty = false;
	return 0;
}

/*
 * al_verify - Verify attribute list integrity.
 */
bool al_verify(struct ntfs_inode *ni)
{
	struct ATTR_LIST_ENTRY *le;
	u16 size;

	if (!ni->attr_list.le)
		return true;

	le = ni->attr_list.le;

	while (le->type != ATTR_END) {
		size = le16_to_cpu(le->size);
		if (!size)
			return false;

		if ((u8 *)le + size >= (u8 *)ni->attr_list.le + ni->attr_list.size)
			return false;

		le = Add2Ptr(le, size);
	}

	return true;
}
