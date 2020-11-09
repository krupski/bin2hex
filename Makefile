####################
# bin2hex Makefile #
####################

TARGET = bin2hex
PKG = $(TARGET)

CC = gcc
STRIP ?= strip
INSTALL = install
PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
CPPFLAGS += -Wall -O3
PROGSIZE = size

$(PKG): $(TARGET)

$(TARGET): $(TARGET).o
	echo "$(TARGET).o -> $(TARGET)"
	$(CC) $(TARGET).o -lm -o $(TARGET)
	$(PROGSIZE) $(TARGET)

$(TARGET).o: $(TARGET).cpp
	echo "$(TARGET).c -> $(TARGET).o"
	$(CC) $(CPPFLAGS) -c $(TARGET).cpp
	$(PROGSIZE) $(TARGET).o

clean:
	rm -f $(TARGET) $(TARGET).o

strip: $(TARGET)
	$(STRIP) $(TARGET)

install: $(TARGET)
	mkdir -p $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(TARGET) $(DESTDIR)$(BINDIR)

debian/changelog:
	#gbp dch --debian-tag='%(version)s' -S -a --ignore-branch -N '$(VERSION)'
	dch --create -v 1.0-1 --package $(PKG)

deb:
	dpkg-buildpackage -b -us -uc
.PHONY: install debian/changelog deb
