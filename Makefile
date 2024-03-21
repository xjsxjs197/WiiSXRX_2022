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

lightrecNoLog.a:
	@echo " "
	@echo "Building lightrecNoLog.a library for PPC"
	@echo " "
	$(MAKE) -C deps/lightrec -f Makefile.NoLog

lightrecWithLog.a:
	@echo " "
	@echo "Building lightrecWithLog.a library for PPC"
	@echo " "
	$(MAKE) -C deps/lightrec -f Makefile.WithLog

zstd.a:
	@echo " "
	@echo "Building zstd.a library for PPC"
	@echo " "
	$(MAKE) -C deps/libchdr/deps/zstd-1.5.5 -f Makefile.wii

lzma.a:
	@echo " "
	@echo "Building lzma.a library for PPC"
	@echo " "
	$(MAKE) -C deps/libchdr/deps/lzma-19.00 -f Makefile.wii

zlibstatic.a:
	@echo " "
	@echo "Building zlibstatic.a library for PPC"
	@echo " "
	$(MAKE) -C deps/libchdr/deps/zlib-1.3.1 -f Makefile.wii

chdrstatic.a:
	@echo " "
	@echo "Building chdrstatic.a library for PPC"
	@echo " "
	$(MAKE) -C deps/libchdr -f Makefile.wii

Wii: lightrecWithLog.a zstd.a lzma.a zlibstatic.a chdrstatic.a
	@$(ECHO) "Building Wii..."
	@$(MAKE) -C Gamecube -f Makefile_Wii

Wii_Release: lightrecNoLog.a zstd.a lzma.a zlibstatic.a chdrstatic.a
	@$(ECHO) "Building Wii_Release..."
	@$(MAKE) -C Gamecube -f Makefile_Wii_Release

#GC:
#	@$(ECHO) "Building GC..."
#	@$(MAKE) -C Gamecube -f Makefile_GC

dist: Wii
	@$(MAKE) -C Gamecube/release/ VERSION=$(VERSION)

clean:
	@$(ECHO) "Cleaning..."
	@$(MAKE) -C deps/lightrec clean -f Makefile.NoLog
	@$(MAKE) -C deps/lightrec clean -f Makefile.WithLog
	@$(MAKE) -C deps/libchdr/deps/zstd-1.5.5 clean -f Makefile.wii
	@$(MAKE) -C deps/libchdr/deps/lzma-19.00 clean -f Makefile.wii
	@$(MAKE) -C deps/libchdr/deps/zlib-1.3.1 clean -f Makefile.wii
	@$(MAKE) -C deps/libchdr clean -f Makefile.wii
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
