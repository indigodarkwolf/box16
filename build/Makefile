#=========================
#
# variables
#
#-------------------------

PROJECT := box16
RELEASE := nr48.1

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
SDL2CONFIG := sdl2-config
MKDIR := mkdir -p

CFLAGS := -Werror $(DFLAGS)
CWARNS := -Wformat -Wformat-security -Wpedantic
CXXFLAGS := --std=c++20 -Werror $(DFLAGS)
CXXWARNS := -Wformat -Wformat-security -Wpedantic

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

LPNG_CXXFLAGS := $(CXXFLAGS) $(CXXWARNS)

LPNG_SRCS := $(LPNG_SRCDIR)/lodepng.cpp
LPNG_OBJS := $(LPNG_OBJDIR)/lodepng.o

#
# RtMidi
#
RTMIDI_SRCDIR := $(VENDORSRC)/rtmidi
RTMIDI_OBJDIR := $(OBJDIR)/rtmidi

RTMIDI_CXXFLAGS := $(CXXFLAGS) $(CXXWARNS)

RTMIDI_SRCS := $(RTMIDI_SRCDIR)/RtMidi.cpp
RTMIDI_OBJS := $(RTMIDI_OBJDIR)/RtMidi.o

#
# ymfm
#
YMFM_SRCDIR := $(VENDORSRC)/ymfm/src
YMFM_OBJDIR := $(OBJDIR)/ymfm

YMFM_CXXFLAGS := $(CXXFLAGS) $(CXXWARNS)

YMFM_SRCS := $(YMFM_SRCDIR)/ymfm_opm.cpp
YMFM_OBJS := $(patsubst $(YMFM_SRCDIR)/%.cpp,$(YMFM_OBJDIR)/%.o,$(YMFM_SRCS))

#
# box16
#
BOX16_SRCDIR := $(REPODIR)/src
BOX16_OBJDIR := $(OBJDIR)/box16/obj

BOX16_OBJDIRS := $(BOX16_OBJDIR) $(BOX16_OBJDIR)/boxmon $(BOX16_OBJDIR)/compat $(BOX16_OBJDIR)/cpu $(BOX16_OBJDIR)/gif $(BOX16_OBJDIR)/glad $(BOX16_OBJDIR)/imgui $(BOX16_OBJDIR)/overlay $(BOX16_OBJDIR)/vera $(BOX16_OBJDIR)/ym2151
BOX16_INCDIRS := -I$(BOX16_SRCDIR) -I$(NFD_SRCDIR)/include -I$(VENDORSRC)/mINI/src/mini -I$(LPNG_SRCDIR) -I$(VENDORSRC)/rtmidi -I$(VENDORSRC)/ymfm/src -I$(VENDORSRC)/fmt-10.2.1/include

BOX16_SRCS := $(wildcard $(BOX16_SRCDIR)/*.cpp) $(wildcard $(BOX16_SRCDIR)/boxmon/*.cpp) $(BOX16_SRCDIR)/compat/compat.cpp $(wildcard $(BOX16_SRCDIR)/cpu/*.cpp) $(wildcard $(BOX16_SRCDIR)/gif/*.cpp) $(wildcard $(BOX16_SRCDIR)/glad/*.cpp) $(wildcard $(BOX16_SRCDIR)/imgui/*.cpp) $(wildcard $(BOX16_SRCDIR)/overlay/*.cpp) $(wildcard $(BOX16_SRCDIR)/vera/*.cpp) $(wildcard $(BOX16_SRCDIR)/ym2151/*.cpp)
BOX16_OBJS := $(patsubst $(BOX16_SRCDIR)/%.cpp,$(BOX16_OBJDIR)/%.o,$(BOX16_SRCS))
BOX16_CXXFLAGS := $(shell $(PKGCONFIG) --cflags alsa gl zlib) $(shell $(SDL2CONFIG) --cflags) $(CXXFLAGS) $(CXXWARNS) $(BOX16_INCDIRS) -include $(BOX16_SRCDIR)/compat/compat.h -DFMT_HEADER_ONLY $(MYFLAGS)
BOX16_LDFLAGS := $(DFLAGS) $(MYFLAGS) $(shell $(PKGCONFIG) --libs alsa gl zlib) $(shell $(SDL2CONFIG) --libs) -ldl

# Detect if CXX is g++ or clang++, in this order.
ifeq '' '$(findstring clang++,$(CXX))'
  BOX16_LDFLAGS += -lstdc++fs
endif

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

verify:
	$(MAKE) clean
	$(MAKE) debug
	$(MAKE) clean
	$(MAKE) trace
	$(MAKE) clean
	$(MAKE) profiling
	$(MAKE) clean
	$(MAKE) all

build: $(OUTDIR)/box16

clean:
	rm -rf $(OBJDIR)
	rm -rf $(RELDIR)

release:
	$(MAKE) zips VSBUILD="Win32"
	$(MAKE) zips VSBUILD="x64"

zips: package
	cd $(RELDIR) && tar -cvf $(RELNAME).tar $(RELNAME)/*
	cd $(RELDIR) && 7zz a -t7z $(RELNAME).7z $(RELNAME)/*
	cd $(RELDIR) && 7zz a -tzip $(RELNAME).zip $(RELNAME)/*
	cd $(RELDIR) && 7zz a -tgzip $(RELNAME).tar.gz $(RELNAME).tar
	cd $(RELDIR) && 7zz a -tbzip2 $(RELNAME).tar.bz2 $(RELNAME).tar

package:
	if [ ! -d $(ZIPSDIR) ]; then $(MKDIR) $(ZIPSDIR); fi
	cp $(VSDIR)/Box16.exe $(ZIPSDIR)/
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
	$(CC) $(NFD_CFLAGS) -c $< -o $@

$(NFD_OBJDIR):
	mkdir -p $@

#
# lodepng
#

$(LPNG_OBJDIR)/%.o: $(LPNG_SRCDIR)/%.cpp | $(LPNG_OBJDIR)
	$(CXX) $(LPNG_CXXFLAGS) -c $< -o $@

$(LPNG_OBJDIR):
	mkdir -p $@

#
# RtMidi
#

$(RTMIDI_OBJDIR)/%.o: $(RTMIDI_SRCDIR)/%.cpp | $(RTMIDI_OBJDIR)
	$(CXX) $(RTMIDI_CXXFLAGS) -c $< -o $@

$(RTMIDI_OBJDIR):
	mkdir -p $@

#
# ymfm
#

$(YMFM_OBJDIR)/%.o: $(YMFM_SRCDIR)/%.cpp | $(YMFM_OBJDIR)
	$(CXX) $(YMFM_CXXFLAGS) -c $< -o $@

$(YMFM_OBJDIR):
	mkdir -p $@

#
# box16
#

$(BOX16_OBJDIR)/%.o: $(BOX16_SRCDIR)/%.cpp | $(BOX16_OBJDIRS)
	$(CXX) $(BOX16_CXXFLAGS) -c $< -o $@

$(BOX16_OBJDIRS):
	mkdir -p $@

$(OUTDIR)/box16: $(BOX16_OBJS) $(NFD_OBJS) $(LPNG_OBJS) $(RTMIDI_OBJS) $(YMFM_OBJS)
	mkdir -p $(OUTDIR)
	$(CXX) $^ -o $@ $(BOX16_LDFLAGS) $(NFD_LDFLAGS)
	cp $(REPODIR)/resources/*.png $(OUTDIR)/
	cp -r $(REPODIR)/resources/r48/* $(OUTDIR)/
