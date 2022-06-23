#This is just here for convenience
#Call "make help" for available commands
ifndef ECHO
ECHO = echo
endif

VERSION = beta2.5

.PHONY:  all Wii Wii_Release

#all:
#	@$(ECHO) "Rebuilding Wii and logging to build.log..."
#	@$(MAKE) -C Gamecube clean -f Makefile_Wii
#	@$(MAKE) -C Gamecube -f Makefile_Wii 2> temp.log
#  #This step removes all leading pathes from the build.log
#	@sed 's|.*wiisxr/Gamecube|/wiisxr/Gamecube|;s|/./|/|;s|\r\n|\n|' temp.log > build.log
#  #note that msys doesn't seem to like sed -i
#	@rm temp.log
all: Wii Wii_Release

Wii:
	@$(ECHO) "Building Wii..."
	@$(MAKE) -C Gamecube -f Makefile_Wii

Wii_Release:
	@$(ECHO) "Building Wii_Release..."
	@$(MAKE) -C Gamecube -f Makefile_Wii_Release

#GC:
#	@$(ECHO) "Building GC..."
#	@$(MAKE) -C Gamecube -f Makefile_GC

dist: Wii
	@$(MAKE) -C Gamecube/release/ VERSION=$(VERSION)

clean:
	@$(ECHO) "Cleaning..."
	@$(MAKE) -C Gamecube clean -f Makefile_Wii
	@$(MAKE) -C Gamecube clean -f Makefile_Wii_Release

help:
	@$(ECHO)
	@$(ECHO) "Available commands:"
	@$(ECHO)
	@$(ECHO) "make            # cleans, rebuilds and log errors (Wii)"
	@$(ECHO) "make Wii        # build everything (Wii)"
	@$(ECHO) "make GC         # build everything (Gamecube)"
	@$(ECHO) "make clean      # clean everything"
