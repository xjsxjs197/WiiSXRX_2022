#This is just here for convenience
#Call "make help" for available commands
ifndef ECHO
ECHO = echo
endif

VERSION = beta2.5

.PHONY: all Wii_Release Wii_Profile Wii

# Production texture-path configuration. Values can be overridden on the make
# command line for diagnostics, for example: make GX_TEXPATH_PHASE1E=0 Wii
GX_TEXPATH_PROFILE ?= 0
GX_TEXPATH_PHASE1A ?= 1
GX_TEXPATH_PHASE1C ?= 1
GX_TEXPATH_PHASE1D ?= 1
GX_TEXPATH_PHASE1E ?= 1
GX_TEXPATH_ATLAS_COUNT ?= 96

export GX_TEXPATH_PROFILE
export GX_TEXPATH_PHASE1A
export GX_TEXPATH_PHASE1C
export GX_TEXPATH_PHASE1D
export GX_TEXPATH_PHASE1E
export GX_TEXPATH_ATLAS_COUNT

ifeq ($(GX_TEXPATH_PHASE1C),1)
ifneq ($(GX_TEXPATH_PHASE1A),1)
$(error GX_TEXPATH_PHASE1C=1 requires GX_TEXPATH_PHASE1A=1)
endif
endif

ifeq ($(GX_TEXPATH_PHASE1E),1)
ifneq ($(GX_TEXPATH_PHASE1D),1)
$(error GX_TEXPATH_PHASE1E=1 requires GX_TEXPATH_PHASE1D=1)
endif
endif

#all:
#	@$(ECHO) "Rebuilding Wii and logging to build.log..."
#	@$(MAKE) -C Gamecube clean -f Makefile_Wii
#	@$(MAKE) -C Gamecube -f Makefile_Wii 2> temp.log
#  #This step removes all leading pathes from the build.log
#	@sed 's|.*wiisxr/Gamecube|/wiisxr/Gamecube|;s|/./|/|;s|\r\n|\n|' temp.log > build.log
#  #note that msys doesn't seem to like sed -i
#	@rm temp.log
all: Wii Wii_Release

opengx.a:
	@echo " "
	@echo "Building opengx.a library for PPC"
	@echo " "
	$(MAKE) -C deps/opengx -f Makefile

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
	$(MAKE) -C deps/libchdr/deps/zstd-1.5.6 -f Makefile.wii

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

Wii: opengx.a lightrecWithLog.a zstd.a lzma.a zlibstatic.a chdrstatic.a
	@$(ECHO) "Building Wii..."
	@$(MAKE) -C Gamecube -f Makefile_Wii

Wii_Profile:
	@$(MAKE) GX_TEXPATH_PROFILE=1 Wii

Wii_Release: opengx.a lightrecNoLog.a zstd.a lzma.a zlibstatic.a chdrstatic.a
	@$(ECHO) "Building Wii_Release..."
	@$(MAKE) -C Gamecube -f Makefile_Wii_Release


#GC:
#	@$(ECHO) "Building GC..."
#	@$(MAKE) -C Gamecube -f Makefile_GC

dist: Wii
	@$(MAKE) -C Gamecube/release/ VERSION=$(VERSION)

clean:
	@$(ECHO) "Cleaning..."
	@$(MAKE) -C deps/opengx clean -f Makefile
	@$(MAKE) -C deps/lightrec clean -f Makefile.NoLog
	@$(MAKE) -C deps/lightrec clean -f Makefile.WithLog
	@$(MAKE) -C deps/libchdr/deps/zstd-1.5.6 clean -f Makefile.wii
	@$(MAKE) -C deps/libchdr/deps/lzma-19.00 clean -f Makefile.wii
	@$(MAKE) -C deps/libchdr/deps/zlib-1.3.1 clean -f Makefile.wii
	@$(MAKE) -C deps/libchdr clean -f Makefile.wii
	@$(MAKE) -C Gamecube clean -f Makefile_Wii
	@$(MAKE) -C Gamecube clean -f Makefile_Wii_Release

help:
	@$(ECHO)
	@$(ECHO) "Available commands:"
	@$(ECHO)
	@$(ECHO) "make             # build normal Wii debug and release versions"
	@$(ECHO) "make Wii         # build normal Wii debug version"
	@$(ECHO) "make Wii_Profile # build debug version with texture profiling"
	@$(ECHO) "make Wii_Release # build normal Wii release version"
	@$(ECHO) "make clean       # clean everything"
	@$(ECHO)
	@$(ECHO) "Production defaults: PROFILE=0, PHASE1A/1C/1D/1E=1, ATLAS_COUNT=96"
	@$(ECHO) "Override with GX_TEXPATH_<name>=<value> on the make command line."
