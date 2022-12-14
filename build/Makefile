#=========================
#
# variables
#
#-------------------------

PROJECT := box16
RELEASE := nr41.1

REPODIR := ..
OBJDIR := obj
SRCDIR := $(REPODIR)/src
OUTDIR := box16

RELDIR := $(REPODIR)/release
RELNAME := box16-$(RELEASE)-$(VSBUILD)
ZIPSDIR := $(RELDIR)/$(RELNAME)
VSDIR := vs2022/out/$(VSBUILD)/Release

VENDORSRC := $(REPODIR)/vendor
VENDOROBJ := $(OBJDIR)/vendor

PKGCONFIG := pkg-config
MKDIR := mkdir -p

CFLAGS := --std=c++20 -Werror $(DFLAGS)
CWARNS := -Wformat -Wformat-security -Wpedantic

#
# nativefiledialog
#
NFD_SRCDIR := $(VENDORSRC)/nativefiledialog/src
NFD_OBJDIR := $(OBJDIR)/nativefiledialog/obj

NFD_CFLAGS := $(shell $(PKGCONFIG) --cflags gtk+-3.0) $(CFLAGS) -I$(NFD_SRCDIR) -I$(NFD_SRCDIR)/include $(MYFLAGS)
NFD_LDFLAGS := $(shell $(PKGCONFIG) --libs gtk+-3.0)

NFD_SRCS := $(NFD_SRCDIR)/nfd_gtk.c $(NFD_SRCDIR)/nfd_common.c
NFD_OBJS := $(NFD_OBJDIR)/nfd_gtk.o $(NFD_OBJDIR)/nfd_common.o

#
# lodepng
#
LPNG_SRCDIR := $(VENDORSRC)/lodepng
LPNG_OBJDIR := $(OBJDIR)/lodepng

LPNG_CFLAGS := $(CFLAGS) $(CWARNS)

LPNG_SRCS := $(LPNG_SRCDIR)/lodepng.cpp
LPNG_OBJS := $(LPNG_OBJDIR)/lodepng.o

#
# RtMidi
#
RTMIDI_SRCDIR := $(VENDORSRC)/rtmidi
RTMIDI_OBJDIR := $(OBJDIR)/rtmidi

RTMIDI_CFLAGS := $(CFLAGS) $(CWARNS)

RTMIDI_SRCS := $(RTMIDI_SRCDIR)/RtMidi.cpp
RTMIDI_OBJS := $(RTMIDI_OBJDIR)/RtMidi.o

#
# ymfm
#
YMFM_SRCDIR := $(VENDORSRC)/ymfm/src
YMFM_OBJDIR := $(OBJDIR)/ymfm

YMFM_CFLAGS := $(CFLAGS) $(CWARNS)

YMFM_SRCS := $(YMFM_SRCDIR)/ymfm_opm.cpp
YMFM_OBJS := $(patsubst $(YMFM_SRCDIR)/%.cpp,$(YMFM_OBJDIR)/%.o,$(YMFM_SRCS))

#
# r8brain-free-src
#
R8BRAIN_SRCDIR := $(VENDORSRC)/r8brain-free-src
R8BRAIN_OBJDIR := $(OBJDIR)/r8brain-free-src

R8BRAIN_CFLAGS := $(CFLAGS) $(CWARNS)
R8BRAIN_LDFLAGS := -lpthread
R8BRAIN_SRCS := $(R8BRAIN_SRCDIR)/r8bbase.cpp
R8BRAIN_OBJS := $(patsubst $(R8BRAIN_SRCDIR)/%.cpp,$(R8BRAIN_OBJDIR)/%.o,$(R8BRAIN_SRCS))

#
# box16
#
BOX16_SRCDIR := $(REPODIR)/src
BOX16_OBJDIR := $(OBJDIR)/box16/obj

BOX16_OBJDIRS := $(BOX16_OBJDIR) $(BOX16_OBJDIR)/compat $(BOX16_OBJDIR)/cpu $(BOX16_OBJDIR)/gif $(BOX16_OBJDIR)/imgui $(BOX16_OBJDIR)/overlay $(BOX16_OBJDIR)/vera $(BOX16_OBJDIR)/ym2151
BOX16_INCDIRS := -I$(BOX16_SRCDIR) -I$(NFD_SRCDIR)/include -I$(VENDORSRC)/mINI/src/mini -I$(LPNG_SRCDIR) -I$(VENDORSRC)/rtmidi -I$(VENDORSRC)/ymfm/src -I$(VENDORSRC)/r8brain-free-src

BOX16_SRCS := $(wildcard $(BOX16_SRCDIR)/*.cpp) $(BOX16_SRCDIR)/compat/compat.cpp $(wildcard $(BOX16_SRCDIR)/cpu/*.cpp) $(wildcard $(BOX16_SRCDIR)/gif/*.cpp) $(wildcard $(BOX16_SRCDIR)/imgui/*.cpp) $(wildcard $(BOX16_SRCDIR)/overlay/*.cpp) $(wildcard $(BOX16_SRCDIR)/vera/*.cpp) $(wildcard $(BOX16_SRCDIR)/ym2151/*.cpp)
BOX16_OBJS := $(patsubst $(BOX16_SRCDIR)/%.cpp,$(BOX16_OBJDIR)/%.o,$(BOX16_SRCS))
BOX16_CFLAGS := $(shell $(PKGCONFIG) --cflags alsa sdl2 gl glew) $(CFLAGS) $(CWARNS) $(BOX16_INCDIRS) -include $(BOX16_SRCDIR)/compat/compat.h $(MYFLAGS)
BOX16_LDFLAGS := $(DFLAGS) $(MYFLAGS) $(shell $(PKGCONFIG) --libs alsa sdl2 gl glew) -lstdc++fs

#=========================
#
# targets
#
#-------------------------

default:
	$(MAKE) all

debug:
	$(MAKE) -j8 build DFLAGS="-g"

trace:
	$(MAKE) -j8 build DFLAGS="-O3 -DTRACE"

profiling:
	$(MAKE) -j8 build DFLAGS="-g -O3 -pg"

all:
	$(MAKE) -j8 build DFLAGS="-O3"

build: $(OUTDIR)/box16

clean:
	rm -rf $(OBJDIR)
	rm -rf $(RELDIR)

release:
	$(MAKE) zips VSBUILD="Win32"
	$(MAKE) zips VSBUILD="x64"

zips: package
	cd $(RELDIR) && tar -cvf $(RELNAME).tar $(RELNAME)/*
	cd $(RELDIR) && 7z a -t7z $(RELNAME).7z $(RELNAME)/*
	cd $(RELDIR) && 7z a -tzip $(RELNAME).zip $(RELNAME)/*
	cd $(RELDIR) && 7z a -tgzip $(RELNAME).tar.gz $(RELNAME).tar
	cd $(RELDIR) && 7z a -tbzip2 $(RELNAME).tar.bz2 $(RELNAME).tar

package:
	if [ ! -d $(ZIPSDIR) ]; then $(MKDIR) $(ZIPSDIR); fi
	cp $(VSDIR)/Box16.exe $(ZIPSDIR)/
	cp $(VSDIR)/glew32.dll $(ZIPSDIR)/
	cp $(VSDIR)/SDL2.dll $(ZIPSDIR)/
	cp $(VSDIR)/box16-icon56-24.png $(ZIPSDIR)/
	cp $(VSDIR)/icons.png $(ZIPSDIR)/
	cp $(VSDIR)/README.box16 $(ZIPSDIR)/

#=========================
#
# recipes
#
#-------------------------

#
# nativefiledialog
#

$(NFD_OBJDIR)/%.o: $(NFD_SRCDIR)/%.c | $(NFD_OBJDIR)
	g++ $(NFD_CFLAGS) -c $< -o $@

$(NFD_OBJDIR):
	mkdir -p $@

#
# lodepng
#

$(LPNG_OBJDIR)/%.o: $(LPNG_SRCDIR)/%.cpp | $(LPNG_OBJDIR)
	g++ $(LPNG_CFLAGS) -c $< -o $@

$(LPNG_OBJDIR):
	mkdir -p $@

#
# RtMidi
#

$(RTMIDI_OBJDIR)/%.o: $(RTMIDI_SRCDIR)/%.cpp | $(RTMIDI_OBJDIR)
	g++ $(RTMIDI_CFLAGS) -c $< -o $@

$(RTMIDI_OBJDIR):
	mkdir -p $@

#
# ymfm
#

$(YMFM_OBJDIR)/%.o: $(YMFM_SRCDIR)/%.cpp | $(YMFM_OBJDIR)
	g++ $(YMFM_CFLAGS) -c $< -o $@

$(YMFM_OBJDIR):
	mkdir -p $@

#
# r8brain-free-src
#

$(R8BRAIN_OBJDIR)/%.o: $(R8BRAIN_SRCDIR)/%.cpp | $(R8BRAIN_OBJDIR)
	g++ $(R8BRAIN_CFLAGS) -c $< -o $@

$(R8BRAIN_OBJDIR):
	mkdir -p $@

#
# box16
#

$(BOX16_OBJDIR)/%.o: $(BOX16_SRCDIR)/%.cpp | $(BOX16_OBJDIRS)
	g++ $(BOX16_CFLAGS) -c $< -o $@

$(BOX16_OBJDIRS):
	mkdir -p $@

$(OUTDIR)/box16: $(BOX16_OBJS) $(NFD_OBJS) $(LPNG_OBJS) $(RTMIDI_OBJS) $(YMFM_OBJS) $(R8BRAIN_OBJS)
	mkdir -p $(OUTDIR)
	g++ $^ -o $@ $(BOX16_LDFLAGS) $(NFD_LDFLAGS) $(R8BRAIN_LDFLAGS)
	cp $(REPODIR)/resources/*.png $(OUTDIR)/
	cp -r $(REPODIR)/resources/r41/* $(OUTDIR)/
