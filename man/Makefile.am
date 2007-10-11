## Process this file with automake to produce Makefile.in
##
## Copyright (C) 1994, 1995 Graeme Wilford.
## Copyright (C) 2001, 2002, 2003, 2007 Colin Watson.
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

LANGUAGES = de.UTF-8 es.UTF-8 fr.UTF-8 it.UTF-8 ja.UTF-8

man1_MANS = \
	man1/apropos.1 \
	man1/lexgrog.1 \
	man1/man.1 \
	man1/manconv.1 \
	man1/manpath.1 \
	man1/whatis.1 \
	man1/zsoelim.1 \
	de.UTF-8/man1/apropos.1 \
	de.UTF-8/man1/man.1 \
	de.UTF-8/man1/manpath.1 \
	de.UTF-8/man1/whatis.1 \
	de.UTF-8/man1/zsoelim.1 \
	es.UTF-8/man1/apropos.1 \
	es.UTF-8/man1/man.1 \
	es.UTF-8/man1/manpath.1 \
	es.UTF-8/man1/whatis.1 \
	es.UTF-8/man1/zsoelim.1 \
	fr.UTF-8/man1/apropos.1 \
	fr.UTF-8/man1/lexgrog.1 \
	fr.UTF-8/man1/man.1 \
	fr.UTF-8/man1/manpath.1 \
	fr.UTF-8/man1/whatis.1 \
	fr.UTF-8/man1/zsoelim.1 \
	it.UTF-8/man1/apropos.1 \
	it.UTF-8/man1/man.1 \
	it.UTF-8/man1/manpath.1 \
	it.UTF-8/man1/whatis.1 \
	it.UTF-8/man1/zsoelim.1 \
	ja.UTF-8/man1/apropos.1 \
	ja.UTF-8/man1/man.1 \
	ja.UTF-8/man1/manpath.1 \
	ja.UTF-8/man1/whatis.1 \
	ja.UTF-8/man1/zsoelim.1
man5_MANS = \
	man5/manpath.5 \
	de.UTF-8/man5/manpath.5 \
	es.UTF-8/man5/manpath.5 \
	fr.UTF-8/man5/manpath.5 \
	it.UTF-8/man5/manpath.5 \
	ja.UTF-8/man5/manpath.5
man8_MANS = \
	man8/accessdb.8 \
	man8/catman.8 \
	man8/mandb.8 \
	de.UTF-8/man8/accessdb.8 \
	de.UTF-8/man8/catman.8 \
	de.UTF-8/man8/mandb.8 \
	es.UTF-8/man8/accessdb.8 \
	es.UTF-8/man8/catman.8 \
	es.UTF-8/man8/mandb.8 \
	fr.UTF-8/man8/accessdb.8 \
	fr.UTF-8/man8/catman.8 \
	fr.UTF-8/man8/mandb.8 \
	it.UTF-8/man8/accessdb.8 \
	it.UTF-8/man8/catman.8 \
	it.UTF-8/man8/mandb.8 \
	ja.UTF-8/man8/accessdb.8 \
	ja.UTF-8/man8/catman.8 \
	ja.UTF-8/man8/mandb.8

SED_ARGS = \
	-e 's,%pager%,$(pager),g' \
        -e 's,%troff%,$(troff),g' \
        -e 's,%date%,$(date),g' \
        -e 's,%version%,$(VERSION),g' \
        -e 's,%zsoelim%,$(TRANS_ZSOELIM),g' \
        -e 's,%man%,$(TRANS_MAN),g' \
        -e 's,%mandb%,$(TRANS_MANDB),g' \
        -e 's,%manpath%,$(TRANS_MANPATH),g' \
        -e 's,%catman%,$(TRANS_CATMAN),g' \
        -e 's,%apropos%,$(TRANS_APROPOS),g' \
        -e 's,%whatis%,$(TRANS_WHATIS),g' \
        -e 's,%manconv%,$(TRANS_MANCONV),g' \
        -e 's,%thzsoelim%,$(TRANS_ZSOELIM_UPPER),g' \
        -e 's,%thman%,$(TRANS_MAN_UPPER),g' \
        -e 's,%thmandb%,$(TRANS_MANDB_UPPER),g' \
        -e 's,%thmanpath%,$(TRANS_MANPATH_UPPER),g' \
        -e 's,%thcatman%,$(TRANS_CATMAN_UPPER),g' \
        -e 's,%thapropos%,$(TRANS_APROPOS_UPPER),g' \
        -e 's,%thwhatis%,$(TRANS_WHATIS_UPPER),g' \
        -e 's,%thmanconv%,$(TRANS_MANCONV_UPPER),g' \
        -e 's,%manpath_config_file%,$(config_file),g'

.man1.1:
	sed $(SED_ARGS) $< > $@
.man5.5:
	sed $(SED_ARGS) $< > $@
.man8.8:
	sed $(SED_ARGS) $< > $@

install-data-local:
	@set -e; \
	for l in . $(LANGUAGES); do \
		for s in 1 5 8; do \
			test -z "$(mandir)" || $(MKDIR_P) "$(DESTDIR)$(mandir)/$$l/man$$s"; \
		done; \
	done; \
	list='$(man1_MANS) $(man8_MANS)'; \
	for i in $$list; do \
		if test -f $(srcdir)/$$i; then \
			file=$(srcdir)/$$i; \
		else \
			file=$$i; \
		fi; \
		ext=`echo $$i | sed -e 's/^.*\\.//'`; \
		inst=`echo $$i | sed -e 's/\\.[0-9a-z]*$$//'`; \
		inst=`echo $$inst | sed -e 's/^.*\///'`; \
		inst=`echo $$inst | sed '$(transform)'`.$$ext; \
		echo " $(INSTALL_DATA) '$$file' '$(DESTDIR)$(mandir)/$$inst'"; \
		$(INSTALL_DATA) "$$file" "$(DESTDIR)$(mandir)/$$inst"; \
	done; \
	list='$(man5_MANS)'; \
	for i in $$list; do
		if test -f $(srcdir)/$$i; then \
			file=$(srcdir)/$$i; \
		else \
			file=$$i; \
		fi; \
		echo " $(INSTALL_DATA) '$$file' '$(DESTDIR)$(mandir)/$$i'"; \
		$(INSTALL_DATA) "$$file" "$(DESTDIR)$(mandir)/$$i"; \
	done

uninstall-local:
	@list='$(man1_MANS) $(man8_MANS)'; \
	for i in $$list; do \
		ext=`echo $$i | sed -e 's/^.*\\.//'`; \
		inst=`echo $$i | sed -e 's/\\.[0-9a-z]*$$//'`; \
		inst=`echo $$inst | sed -e 's/^.*\///'`; \
		inst=`echo $$inst | sed '$(transform)'`.$$ext; \
		echo " rm -f '$(DESTDIR)$(mandir)/$$inst'"; \
		rm -f "$(DESTDIR)$(mandir)/$$inst"; \
	done; \
	list='$(man5_MANS)'; \
	for i in $$list; do
		echo " rm -f '$(DESTDIR)$(mandir)/$$i'"; \
		rm -f "$(DESTDIR)$(mandir)/$$i"; \
	done

mostlyclean-local:
	rm -f index.* */index.*

dist-hook:
	@list='$(man1_MANS) $(man5_MANS) $(man8_MANS)'; \
	dist_files=`for file in $$list; do \
			echo $$file | sed 's/\.\([0-9]\)/.man\1/'; \
		    done`; \
	$(MKDIR_P) `echo "$$dist_files" | \
		    sed 's|^|$(distdir)/|;s,/[^/]*$$,,' | \
		    sort -u`; \
	for file in $$dist_files; do \
		test -f $(distdir)/$$file \
		|| cp -p ./$$file $(distdir)/$$file \
		|| exit 1; \
	done
