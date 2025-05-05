#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

include $(DEVKITPPC)/wii_rules

INCLUDES := ./

# Directories.
OBJ_DIR			:= obj
LIB_DIR			:= lib
OPENGX_OBJ_DIR		:= $(OBJ_DIR)
OPENGX_SRC_DIR		:= ./
INSTALL_HEADER_DIR	?= $(PORTLIBS_PATH)/wii/include
INSTALL_LIB_DIR		?= $(PORTLIBS_PATH)/wii/lib

# Tools.
PIPE_TO_SED := 2>&1 | sed "s/:\([0-9]*\):/\(\1\) :/"

# Library source files.
SDL_SRCS	:= \
	$(wildcard $(OPENGX_SRC_DIR)/gc_gl.c) \
	$(wildcard $(OPENGX_SRC_DIR)/image_DXT.c) \
	$(wildcard $(OPENGX_SRC_DIR)/pixels.c) 

# Library object files.
OPENGX_OBJS	:= $(subst $(OPENGX_SRC_DIR),$(OPENGX_OBJ_DIR),$(SDL_SRCS:.c=.o))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			        -I$(LIBOGC_INC)

# What's a full build?
all: $(LIB_DIR)/libOpengx.a

# How to delete the intermediate files.
clean:
	@echo Cleaning $(LIB_DIR) $(OPENGX_OBJS)
	@rm -fr $(LIB_DIR) $(OPENGX_OBJS)

install: all
	@mkdir -p $(INSTALL_HEADER_DIR) $(INSTALL_LIB_DIR)
	@mkdir -p $(INSTALL_HEADER_DIR)/SDL
	@cp -frv $(LIB_DIR)/*.* $(INSTALL_LIB_DIR)
	@cp -frv include/*.* $(INSTALL_HEADER_DIR)/SDL

# Used to install library by pressing Alt+R in Programmer's Notepad
run: all install

# How to build a library.
$(LIB_DIR)/libOpengx.a: $(OPENGX_OBJS)
	@echo Archiving $@
	@-mkdir -p $(dir $@)
	powerpc-eabi-ar crs $@ $(OPENGX_OBJS)
	@echo ----

# Compilation flags.
COMMON_FLAGS	:= -g -O2 -w $(MACHDEP) -fvisibility=hidden
INCLUDES		:= -Iinclude -I$(LIBOGC_INC)
DEFINES			:= -DGEKKO
CFLAGS			:= $(COMMON_FLAGS) $(INCLUDES) $(INCLUDE) $(DEFINES)--pedantic -D__BIG_ENDIAN -D__powerpc__ -DBIG_ENDIAN -DLOG_LEVEL=0 -DNDEBUG

# Test link flags.
LDFLAGS			:= $(COMMON_FLAGS) -L$(LIB_DIR) -L$(LIBOGC_LIB) $(PORTLIBS_PATH)/wii/lib

# How to compile C file (SDL library).
$(OPENGX_OBJ_DIR)/%.o: $(OPENGX_SRC_DIR)/%.c
	@echo Compiling $<
	@-mkdir -p $(dir $@)
	powerpc-eabi-gcc $(CFLAGS) -c $< -o $@ $(PIPE_TO_SED)
