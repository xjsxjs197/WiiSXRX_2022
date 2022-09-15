#Creates a WiiStation Redistributable
ifndef ECHO
ECHO = echo
endif

VERSION = 1.0beta

.PHONY:  all

all: dist

dist: ../WiiStation_Release.dol clean
	@$(ECHO) "Making Redistributable..."
	@cp ../WiiStation_Release.dol apps/WiiStation/boot.dol
	@mkdir -p \
		wiisxrx/bios \
		wiisxrx/saves \
		wiisxrx/isos
	@zip -r ../../WiiStation-$(VERSION).zip wiisxrx apps README
 
clean:
	@$(ECHO) "Cleaning..."
	@rm -f apps/WiiStation/boot.dol ../../WiiStation-*.zip