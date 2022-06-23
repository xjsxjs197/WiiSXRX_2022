#Creates a WiiSXR Redistributable
ifndef ECHO
ECHO = echo
endif

VERSION = beta2.2

.PHONY:  all

all: dist

dist: ../WiiSXRX.dol clean
	@$(ECHO) "Making Redistributable..."
	@cp ../WiiSXRX.dol apps/WiiSXRX/boot.dol
	@mkdir -p \
		wiisxrx/bios \
		wiisxrx/saves \
		wiisxrx/isos
	@zip -r ../../WiiSXRX-$(VERSION).zip wiisxrx apps README
 
clean:
	@$(ECHO) "Cleaning..."
	@rm -f apps/WiiSXRX/boot.dol ../../WiiSXRX-*.zip