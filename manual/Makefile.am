## Process this file with automake to produce Makefile.in
##
## Copyright (C) 1995 Graeme Wilford.
## Copyright (C) 2001, 2007 Colin Watson.
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

ME_FILES = \
	comp.me \
	db.me \
	files.me \
	format.me \
	glossary.me \
	intro.me \
	leader.me \
	man_db.me \
	misc.me \
	sec.me

MANUAL = man_db

dist_noinst_DATA = \
	README \
	$(ME_FILES)
noinst_DATA = \
	$(MANUAL).cat \
	$(MANUAL).dvi \
	$(MANUAL).ps

CLEANFILES = \
	$(noinst_DATA) \
	$(MANUAL).html \
	$(MANUAL).pp \
	$(MANUAL).tdvi \
	$(MANUAL).tps \
	version

SOELIM = soelim

version: $(top_srcdir)/configure.ac
	echo '.ds V $(roff_version)' > $@

$(MANUAL).pp: $(ME_FILES) version
	$(SOELIM) -I$(srcdir) man_db.me | tbl > $@

.pp.dvi:
	groff -me -Tdvi $< > $@

.pp.ps:
	groff -me -Tps $< > $@

.pp.tdvi:
	troff -me -Tdvi $< | grodvi -d > $@

.tdvi.tps:
	dvips -f -t a4 $< > $@

.pp.cat:
	nroff -me $< > $@

.pp.html:
	groff -me -Thtml -P-I$* $< > $@

# Automake defines these targets already, so we need to use -local.
dvi-local: $(MANUAL).dvi
ps-local: $(MANUAL).ps
html-local: $(MANUAL).html

# These are all ours.
cat: $(MANUAL).cat
tps: $(MANUAL).tps

clean-local:
	-rm -f $(MANUAL)-*.png
