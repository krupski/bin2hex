TARGET     = bin2hex
PKG        = $(TARGET)

CC        ?= gcc
STRIP     ?= strip
INSTALL   ?= install
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

ifeq ($(TARGET_OS), MinGW)
EXEC_SUFFIX := .exe
CPPFLAGS += -posix
CPPFLAGS += -Dffs=__builtin_ffs
CPPFLAGS += -D__USE_MINGW_ANSI_STDIO=1
endif

SRC = $(TARGET).cpp

$(PKG): $(TARGET)$(EXEC_SUFFIX)

$(TARGET)$(EXEC_SUFFIX): $(SRC)
	$(CC) $(CPPFLAGS) $(SRC) $(LDFLAGS) -o $@

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
