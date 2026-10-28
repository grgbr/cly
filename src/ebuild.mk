################################################################################
# SPDX-License-Identifier: LGPL-3.0-only
#
# This file is part of cly.
# Copyright (C) 2026 Grégor Boirie <gregor.boirie@free.fr>
################################################################################

common-cflags  := -Wall \
                  -Wextra \
                  -Wformat=2 \
                  -Wundef \
                  -Wshadow \
                  -Wmissing-declarations \
                  -D_GNU_SOURCE \
                  $(EXTRA_CFLAGS) \
                  -I$(TOPDIR) \
                  -fvisibility=internal

# This generates spurious warnings with GCC 8.x. Keep this disabled for now...
#common-cflags += -Wcast-align

common-ldflags := $(common-cflags) $(EXTRA_LDFLAGS) \
                  -Wl,--as-needed \
                  -Wl,-z,start-stop-visibility=internal

ifneq ($(filter y,$(CONFIG_CLY_ASSERT)),)
common-cflags  := $(filter-out -DNDEBUG,$(common-cflags))
common-ldflags := $(filter-out -DNDEBUG,$(common-ldflags))
endif # ($(filter y,$(CONFIG_CLY_ASSERT)),)

bins           := cly
cly-objs       := common.o \
                  node.o \
                  arg.o \
                  match.o \
                  expr.o \
                  shell.o \
                  \
                  path.o \
                  cmd.o \
                  dir.o \
                  \
                  table.o \
                  srepo_table.o \
                  show.o \
                  \
                  list.o \
                  cd.o \
                  pwd.o \
                  find.o \
                  xpath.o \
                  schema.o \
                  edit.o \
                  commit.o \
                  discard.o \
                  quit.o \
                  \
                  build.o \
                  cly.o
cly-cflags     := $(common-cflags)
cly-ldflags    := $(common-ldflags)
cly-pkgconf    := libsrepo smartcols libutils readline tinfo \
                  $(call kconf_enabled,CLY_LOG,libelog)

# ex: filetype=make :
