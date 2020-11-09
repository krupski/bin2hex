TARGET     = bin2hex
PKG        = $(TARGET)

CC        ?= gcc
STRIP     ?= strip
INSTALL   ?= install
PROGSIZE  ?= size
PREFIX    ?= /usr
BINDIR    ?= $(PREFIX)/bin
CPPFLAGS  ?= -Wall -O3
LDFLAGS   ?= -lm
WARNERROR ?= no

ifeq ($(WARNERROR), yes)
CPPFLAGS += -Werror
endif

ifeq ($(CONFIG_STATIC),yes)
LDFLAGS += -static
endif

# FIXME: Should we check for Cygwin/MSVC as well?
ifeq ($(TARGET_OS), MinGW)
EXEC_SUFFIX := .exe
# MinGW doesn't have the ffs() function, but we can use gcc's __builtin_ffs().
CPPFLAGS += -Dffs=__builtin_ffs
# Some functions provided by Microsoft do not work as described in C99 specifications. This macro fixes that
# for MinGW. See http://sourceforge.net/p/mingw-w64/wiki2/printf%20and%20scanf%20family/ */
CPPFLAGS += -D__USE_MINGW_ANSI_STDIO=1
endif

$(PKG): $(TARGET)$(EXEC_SUFFIX)

$(TARGET)$(EXEC_SUFFIX): $(TARGET).cpp
	echo "$(TARGET).cpp -> $(TARGET)$(EXEC_SUFFIX)"
	$(CC) $(CPPFLAGS) $(TARGET).cpp $(LDFLAGS) -o $(TARGET)$(EXEC_SUFFIX)
	$(PROGSIZE) $(TARGET)$(EXEC_SUFFIX)

clean:
	rm -f $(TARGET)$(EXEC_SUFFIX)

strip: $(TARGET)$(EXEC_SUFFIX)
	$(STRIP) $(TARGET)$(EXEC_SUFFIX)

install: $(TARGET)
	mkdir -p $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(TARGET) $(DESTDIR)$(BINDIR)

debian/changelog:
	#gbp dch --debian-tag='%(version)s' -S -a --ignore-branch -N '$(VERSION)'
	dch --create -v 1.0-1 --package $(PKG)

deb:
	dpkg-buildpackage -b -us -uc
.PHONY: install debian/changelog deb
