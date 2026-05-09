# SPDX-License-Identifier: GPL-2.0
#
# Makefile for NTFS3 filesystem driver
#
# Copyright (c) 2020-2024 Paragon Software GmbH
# Backport to sslinuX-4.20

obj-$(CONFIG_NTFS3_FS) += ntfs3.o

ntfs3-objs := super.o \
              inode.o \
              file.o \
              record.o \
              attr.o \
              run.o \
              lznt.o \
              bitfunc.o

# Compiler flags for 4.20 compatibility
ccflags-y := -D__Linux__=1