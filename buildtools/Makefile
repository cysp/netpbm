ifeq ($(SRCDIR)x,x)
  SRCDIR = $(CURDIR)/..
  BUILDDIR = $(SRCDIR)
endif
SUBDIR = buildtools
VPATH = .:$(SRCDIR)/$(SUBDIR)
include $(BUILDDIR)/config.mk

MERGE_OBJECTS =

PROGS = libopt typegen endiangen

all: $(PROGS)

OMIT_BUILDTOOL_RULE = 1
include $(SRCDIR)/common.mk

ifdef DLLVER
STRIP_DLL_VERSION=-DDLLVERSTR="\"$(DLLVER)\""
endif

ifeq ($(LINKER_CAN_DO_EXPLICIT_LIBRARY),Y)
EXPLICIT=-DEXPLICIT
endif

libopt.o: libopt.c
	$(CC_FOR_BUILD) -c -o $@ $(CFLAGS_FOR_BUILD) \
	  -DSHLIBPREFIXLIST="\"$(SHLIBPREFIXLIST)\"" \
	  $(STRIP_DLL_VERSION) $(EXPLICIT) \
	  $(CFLAGS_PERSONAL) $(CFLAGS) $(CADD) \
	  $<

typegen.o endiangen.o:%.o:%.c
	$(CC_FOR_BUILD) -c -o $@ $(CFLAGS_FOR_BUILD) $<

$(BUILDPROGS):%:%.o
	$(LD_FOR_BUILD) -o $@ $(LDFLAGS_FOR_BUILD) $<

distclean clean: cleanlocal
.PHONY: cleanlocal
cleanlocal:
	rm -f $(PROGS)
