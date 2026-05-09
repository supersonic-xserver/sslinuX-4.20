// SPDX-License-Identifier: GPL-2.0
/*
 * attr.c - NTFS3 attribute operations
 *
 * Copyright (c) 2020-2024 Paragon Software GmbH
 * Backport to sslinuX-4.20
 */

#include "ntfs3.h"

/*
 * ntfs3_attr_size - Get attribute size
 */
size_t ntfs3_attr_size(struct ATTR *attr)
{
	if (attr->resident)
		return le32_to_cpu(attr->size) + sizeof(struct ATTR);
	else
		return le32_to_cpu(attr->size) + 0x48; /* non-resident header size */
}

/*
 * ntfs3_attr_list_entry_size - Get attribute list entry size
 */
size_t ntfs3_attr_list_entry_size(struct ATTR_LIST_ENTRY *entry)
{
	return le16_to_cpu(entry->size);
}

/*
 * ntfs3_first_attr - Get first attribute in record
 */
struct ATTR *ntfs3_first_attr(struct MFT_RECORD *rec)
{
	return (struct ATTR *)((char *)rec + le16_to_cpu(rec->attrs_offset));
}

/*
 * ntfs3_next_attr - Get next attribute
 */
struct ATTR *ntfs3_next_attr(struct ATTR *attr)
{
	return (struct ATTR *)((char *)attr + le32_to_cpu(attr->size));
}

/*
 * ntfs3_is_attr - Check if it's a valid attribute
 */
bool ntfs3_is_attr(struct ATTR *attr)
{
	if (!attr)
		return false;
	return le32_to_cpu(attr->type) <= AT_LOGGED_UTILITY_STREAM;
}

/*
 * ntfs3_find_attr - Find attribute in record
 */
struct ATTR *ntfs3_find_attr(struct MFT_RECORD *rec, enum ATTR_TYPE type)
{
	struct ATTR *attr = ntfs3_first_attr(rec);
	
	while (ntfs3_is_attr(attr)) {
		if (le32_to_cpu(attr->type) == type)
			return attr;
		attr = ntfs3_next_attr(attr);
	}
	
	return NULL;
}