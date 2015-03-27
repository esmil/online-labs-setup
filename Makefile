MAKEFLAGS     += -rR
CROSS_COMPILE  =
CC             = $(CROSS_COMPILE)gcc -std=gnu99
CFLAGS         = -Os -pipe -Wall -Wextra -D_GNU_SOURCE
SED            = sed
STRIP          = strip -s
INSTALL        = install

pkgname  = online-labs-setup
prefix   = /usr
libdir   = $(prefix)/lib
systemd  = $(libdir)/systemd


.PHONY: all clean install

.PRECIOUS: %.gen

all: shutdown

test: CFLAGS += -DTEST
test: shutdown.c
	$(CC) $(CFLAGS) -o $@ -static $(LDFLAGS) $<

shutdown: shutdown.c
	$(CC) $(CFLAGS) -o $@ -static $(LDFLAGS) $<

oc-nbd-generator.gen: oc-nbd-generator
	$(SED) -e '/^. metadata$$/c. $(libdir)/$(pkgname)/metadata' $< > $@

set-root-sshkeys.gen: set-root-sshkeys
	$(SED) -e '/^. metadata$$/c. $(libdir)/$(pkgname)/metadata' $< > $@

%.service.gen: %.service
	$(SED) -e 's|@libexec@|$(libdir)/$(pkgname)|' $< > $@

$(DESTDIR)$(libdir)/$(pkgname):
	$(INSTALL) -d '$@'

$(DESTDIR)$(systemd)/system-generators:
	$(INSTALL) -d '$@'

$(DESTDIR)$(systemd)/system-preset:
	$(INSTALL) -d '$@'

$(DESTDIR)$(systemd)/system:
	$(INSTALL) -d '$@'

$(DESTDIR)$(libdir)/$(pkgname)/metadata: metadata | $(DESTDIR)$(libdir)/$(pkgname)
	$(INSTALL) -m644 $< '$@'

$(DESTDIR)$(libdir)/$(pkgname)/shutdown: shutdown | $(DESTDIR)$(libdir)/$(pkgname)
	$(STRIP) $<
	$(INSTALL) -m755 $< '$@'

$(DESTDIR)$(libdir)/$(pkgname)/sync-kernel-modules: sync-kernel-modules | $(DESTDIR)$(libdir)/$(pkgname)
	$(INSTALL) -m755 $< '$@'

$(DESTDIR)$(libdir)/$(pkgname)/%: %.gen | $(DESTDIR)$(libdir)/$(pkgname)
	$(INSTALL) -m755 $< '$@'

$(DESTDIR)$(systemd)/system-generators/%: %.gen | $(DESTDIR)$(systemd)/system-generators
	$(INSTALL) -m755 $< '$@'

$(DESTDIR)$(systemd)/system-preset/85-online-labs.preset: online-labs.preset | $(DESTDIR)$(systemd)/system-preset
	$(INSTALL) -m755 $< '$@'

$(DESTDIR)$(systemd)/system/%.service: %.service.gen | $(DESTDIR)$(systemd)/system
	$(INSTALL) -m644 $< '$@'

install: \
	$(DESTDIR)$(libdir)/$(pkgname)/shutdown \
	$(DESTDIR)$(libdir)/$(pkgname)/metadata \
	$(DESTDIR)$(libdir)/$(pkgname)/set-root-sshkeys \
	$(DESTDIR)$(libdir)/$(pkgname)/sync-kernel-modules \
	$(DESTDIR)$(systemd)/system-generators/oc-nbd-generator \
	$(DESTDIR)$(systemd)/system-preset/85-online-labs.preset \
	$(DESTDIR)$(systemd)/system/oc-set-root-sshkeys.service \
	$(DESTDIR)$(systemd)/system/oc-sync-kernel-modules.service \
	$(DESTDIR)$(systemd)/system/oc-pre-shutdown.service

clean:
	rm -f shutdown test *.gen
