# Declaration of the programs:
bin_PROGRAMS = dvdcopy secdump
dvdcopy_SOURCES = src/main.cc src/headers.hh \
	src/dvdcopy.hh src/dvdcopy.cc \
	src/dvdoutfile.hh src/dvdoutfile.cc \
	src/dvdreader.hh src/dvdreader.cc \
	src/dvdfile.hh src/dvdfile.cc \
	src/dvddrive.hh src/dvddrive.cc \
	src/badsectors.hh src/badsectors.cc

secdump_SOURCES = src/secdump.cc

# HTML
CLEANFILES = README.html

all-local: README.html

install-data-hook:
	if [ -f "README.html" ] ; then \
	    $(MKDIR_P) $(DESTDIR)$(docdir) && \
	    $(INSTALL_DATA) README.html $(DESTDIR)$(docdir) ;\
	fi

README.html:
	HTMLTITLE="dvdcopy: backup your DVDs to disk" ;\
	if [ -x "$(PANDOC)" ] ; then \
	    $(PANDOC) -f textile -t html -s --ascii README.textile | \
	    sed -e "s@<title></title>@<title>$$HTMLTITLE</title>@" >$@ ;\
	fi
