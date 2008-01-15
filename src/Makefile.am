## Process this file with automake to produce Makefile.in
##
## Copyright (C) 1994, 1995 Graeme Wilford.
## Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007 Colin Watson.
##
## This file is part of man-db.
##
## man-db is free software; you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## man-db is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with man-db; if not, write to the Free Software Foundation,
## Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

pkglibexecdir = $(libexecdir)/@PACKAGE@

bin_PROGRAMS = \
	catman \
	lexgrog \
	man \
	mandb \
	manpath \
	whatis \
	zsoelim
sbin_PROGRAMS = accessdb
pkglibexec_PROGRAMS = globbing manconv
noinst_PROGRAMS = wrapper
noinst_DATA = man_db.conf

AM_CPPFLAGS = \
	-I$(top_builddir)/include \
	-I$(top_builddir)/gnulib/lib \
	-I$(top_srcdir)/gnulib/lib \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/libdb \
	-DCONFIG_FILE=\"$(config_file)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DAPROPOS=\"$(bindir)/$(TRANS_APROPOS)\" \
	-DAPROPOS_NAME=\"$(TRANS_APROPOS)\" \
	-DMAN=\"$(bindir)/$(TRANS_MAN)\" \
	-DMANCONV=\"$(pkglibexecdir)/$(TRANS_MANCONV)\" \
	-DMANDB=\"$(bindir)/$(TRANS_MANDB)\" \
	-DSOELIM=\"$(bindir)/$(TRANS_ZSOELIM)\" \
	-DWHATIS=\"$(bindir)/$(TRANS_WHATIS)\"

LIBMAN = $(top_builddir)/lib/libman.a $(top_builddir)/gnulib/lib/libgnu.a
LIBMANDB = $(top_builddir)/libdb/libmandb.a $(DBLIBS)

accessdb_LDADD = $(LIBMANDB) $(LIBMAN)
catman_LDADD = $(LIBMANDB) $(LIBMAN)
globbing_LDADD = $(LIBMAN)
lexgrog_LDADD = $(LIBMAN) $(LIBCOMPRESS)
man_LDADD = $(LIBMANDB) $(LIBMAN) $(LIBCOMPRESS)
manconv_LDADD = $(LIBMAN) $(LIBCOMPRESS)
mandb_LDADD = $(LIBMANDB) $(LIBMAN) $(LIBCOMPRESS)
manpath_LDADD = $(LIBMAN)
whatis_LDADD = $(LIBMANDB) $(LIBMAN) $(LIBICONV)
wrapper_LDADD = $(LIBMAN)
zsoelim_LDADD = $(LIBMAN) $(LIBCOMPRESS)

accessdb_SOURCES = \
	accessdb.c \
	util.c
catman_SOURCES = \
	catman.c \
	fake_security.c \
	manp.c \
	manp.h \
	util.c
globbing_SOURCES = \
	globbing.c \
	globbing.h \
	globbing_test.c \
	util.c
lexgrog_SOURCES = \
	compression.c \
	descriptions.c \
	descriptions.h \
	encodings.c \
	encodings.h \
	fake_security.c \
	lexgrog.l \
	lexgrog_test.c \
	security.h \
	ult_src.c \
	ult_src.h \
	util.c
man_SOURCES = \
	compression.c \
	convert_name.c \
	convert_name.h \
	encodings.c \
	encodings.h \
	filenames.c \
	filenames.h \
	globbing.c \
	globbing.h \
	man.c \
	man.h \
	manp.c \
	manp.h \
	security.c \
	security.h \
	ult_src.c \
	ult_src.h \
	util.c
manconv_SOURCES = \
	manconv.c
mandb_SOURCES = \
	check_mandirs.c \
	check_mandirs.h \
	compression.c \
	descriptions.c \
	descriptions.h \
	descriptions_store.c \
	encodings.c \
	encodings.h \
	filenames.c \
	filenames.h \
	globbing.c \
	globbing.h \
	lexgrog.l \
	mandb.c \
	manp.c \
	manp.h \
	security.c \
	security.h \
	straycats.c \
	ult_src.c \
	ult_src.h \
	util.c
manpath_SOURCES = \
	fake_security.c \
	manp.c \
	manp.h \
	manpath.c \
	util.c
whatis_SOURCES = \
	encodings.c \
	encodings.h \
	fake_security.c \
	manp.c \
	manp.h \
	util.c \
	whatis.c
wrapper_SOURCES = \
	wrapper.c
zsoelim_SOURCES = \
	zsoelim.l

CLEANFILES = apropos man_db.conf

EXTRA_DIST = \
	README.lexgrog \
	README.zsoelim \
	lexgrog.l-2.3.8 \
	lexgrog.l-2.4a \
	lexgrog.l.new

apropos$(EXEEXT): whatis$(EXEEXT)
	$(LN_S) $< $@

all-am: apropos$(EXEEXT)

install-exec-hook:
	-test -z "$(man_owner)" || \
	chown $(man_owner) \
		$(DESTDIR)$(bindir)/$(TRANS_MAN) \
		$(DESTDIR)$(bindir)/$(TRANS_MANDB)
	chmod $(man_mode) \
		$(DESTDIR)$(bindir)/$(TRANS_MAN) \
		$(DESTDIR)$(bindir)/$(TRANS_MANDB)
	cd $(DESTDIR)$(bindir) && rm -f $(TRANS_APROPOS)$(EXEEXT) && \
		$(LN_S) $(TRANS_WHATIS)$(EXEEXT) $(TRANS_APROPOS)$(EXEEXT)

install-data-hook:
	@if test -f $(DESTDIR)$(config_file); then \
		echo "$(DESTDIR)$(config_file) already exists; overwrite manually if necessary"; \
	else \
		test -z "$(config_file_dirname)" || $(MKDIR_P) "$(DESTDIR)$(config_file_dirname)"; \
		echo " $(INSTALL_DATA) man_db.conf $(DESTDIR)$(config_file)"; \
		$(INSTALL_DATA) man_db.conf $(DESTDIR)$(config_file); \
	fi

uninstall-hook:
	rm -f $(DESTDIR)$(bindir)/$(TRANS_APROPOS)$(EXEEXT)
	@echo "Please remove $(DESTDIR)$(config_file) manually if necessary"
