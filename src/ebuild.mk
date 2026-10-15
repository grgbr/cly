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
                  quit.o \
                  \
                  build.o \
                  cli.o
cly-cflags     := $(common-cflags)
cly-ldflags    := $(common-ldflags)
cly-pkgconf    := libsrepo smartcols readline tinfo

# ex: filetype=make :

#CFLAGS  := -Wall -Wextra \
#           -fsanitize=address \
#           -fsanitize=pointer-compare \
#           -fsanitize=pointer-subtract \
#           -fsanitize=leak \
#           -fsanitize=undefined \
#           $(shell env PKG_CONFIG_PATH='$(PKG_CONFIG_PATH)' \
#                       $(PKG_CONFIG) --cflags sysrepo libyang smartcols readline tinfo) \
#           $(EXTRA_CFLAGS)
#LDFLAGS := $(shell env PKG_CONFIG_PATH='$(PKG_CONFIG_PATH)' \
#                       $(PKG_CONFIG) --libs sysrepo libyang smartcols readline tinfo) \
#           $(EXTRA_LDFLAGS)
#
#.PHONY: all
#all: build
#
#.PHONY: install
#install: $(DESTDIR)$(PREFIX)/bin/nmgmt-schema \
#         $(DESTDIR)$(PREFIX)/bin/nmgmt-data \
#         $(DESTDIR)$(PREFIX)/bin/nmgmt-cli
#$(DESTDIR)$(PREFIX)/bin/nmgmt-schema: $(BUILDDIR)/nmgmt-schema
#	$(INSTALL) --mode=755 -D $(<) $(@)
#$(DESTDIR)$(PREFIX)/bin/nmgmt-data: $(BUILDDIR)/nmgmt-data
#	$(INSTALL) --mode=755 -D $(<) $(@)
#$(DESTDIR)$(PREFIX)/bin/nmgmt-cli: $(BUILDDIR)/nmgmt-cli
#	$(INSTALL) --mode=755 -D $(<) $(@)
#
#.PHONY: build
#build: $(BUILDDIR)/nmgmt-schema \
#	$(BUILDDIR)/nmgmt-data \
#	$(BUILDDIR)/nmgmt-cli
#$(BUILDDIR)/nmgmt-schema: old/schema.c | $(BUILDDIR)/
#	$(CC) -o $(@) $(CFLAGS) $(<) $(LDFLAGS)
#$(BUILDDIR)/nmgmt-data: old/data.c | $(BUILDDIR)/
#	$(CC) -o $(@) $(CFLAGS) $(<) $(LDFLAGS)
#$(BUILDDIR)/nmgmt-cli: cli.c build.c show.c lyd_table.c list.c cd.c pwd.c find.c xpath.c schema.c quit.c table.c shell.c match.c expr.c yang.c dir.c cmd.c path.c arg.c node.c common.c | $(BUILDDIR)/
#	$(CC) -dM -o $(@) $(CFLAGS) $(^) $(LDFLAGS)
#	
#.PHONY: clean
#clean:
#	$(RM) $(DESTDIR)$(PREFIX)/bin/nmgmt-schema
#	$(RM) $(DESTDIR)$(PREFIX)/bin/nmgmt-data
#	$(RM) $(DESTDIR)$(PREFIX)/bin/nmgmt-clin
#	$(RM) -r $(BUILDDIR)
#
#$(BUILDDIR)/:
#	$(MKDIR) -p $(@)
#
#.PHONY: dist
#dist:
#
#.PHONY: distclean
#distclean:
