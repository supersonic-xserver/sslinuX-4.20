# SPDX-License-Identifier: GPL-2.0
#
# Makefile for NTFS3 filesystem driver
#
# Copyright (c) 2020-2024 Paragon Software GmbH
# Backport to sslinuX-4.20

obj-$(CONFIG_NTFS3_FS) += ntfs3.o

ntfs3-objs := \
	attrib.o \
	attrlist.o \
	bitfunc.o \
	bitmap.o \
	dir.o \
	file.o \
	frecord.o \
	fslog.o \
	fsntfs.o \
	index.o \
	inode.o \
	lznt.o \
	namei.o \
	record.o \
	run.o \
	super.o \
	upcase.o \
	xattr.o

# Compiler flags for 4.20 compatibility
ccflags-y := -D__Linux__=1
ccflags-$(CONFIG_CC_IS_CLANG) += -Wno-unused-function
