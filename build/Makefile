PROJECT := box16

REPODIR := ..
OBJDIR := obj
VENDORDIR := $(REPODIR)/vendor
OUTDIR := box16

SDL2CONFIG := /usr/bin/sdl2-config
PKGCONFIG := pkg-config

NFD_SRCDIR := $(VENDORDIR)/nativefiledialog/src
NFD_OBJDIR := $(OBJDIR)/nativefiledialog/obj

NFD_CFLAGS := $(shell $(PKGCONFIG) --cflags gtk+-3.0) -I$(NFD_SRCDIR) -I$(NFD_SRCDIR)/include -m64
NFD_LDFLAGS := $(shell $(PKGCONFIG) --libs gtk+-3.0) -m64
NFD_ARFLAGS := rcs

NFD_SRCS := $(NFD_SRCDIR)/nfd_gtk.c $(NFD_SRCDIR)/nfd_common.c
NFD_OBJS := $(NFD_OBJDIR)/nfd_gtk.o $(NFD_OBJDIR)/nfd_common.o

BOX16_SRCDIR := $(REPODIR)/src
BOX16_OBJDIR := $(OBJDIR)/box16/obj

BOX16_SRCS := $(wildcard $(BOX16_SRCDIR)/*.cpp) $(BOX16_SRCDIR)/compat/compat.cpp $(wildcard $(BOX16_SRCDIR)/cpu/*.cpp) $(wildcard $(BOX16_SRCDIR)/gif/*.cpp) $(wildcard $(BOX16_SRCDIR)/imgui/*.cpp) $(wildcard $(BOX16_SRCDIR)/overlay/*.cpp) $(wildcard $(BOX16_SRCDIR)/vera/*.cpp) $(wildcard $(BOX16_SRCDIR)/ym2151/*.cpp)
BOX16_OBJS := $(patsubst $(BOX16_SRCDIR)/%.cpp,$(BOX16_OBJDIR)/%.o,$(BOX16_SRCS))
BOX16_CFLAGS := $(shell $(SDL2CONFIG) --cflags) --std=c++17 -O3 -Wformat -Wformat-security -m64 -I$(BOX16_SRCDIR) -I$(NFD_SRCDIR)/include -I$(VENDORDIR)/mINI/src/mini -include $(BOX16_SRCDIR)/compat/compat.h
BOX16_LDFLAGS := $(shell $(SDL2CONFIG) --libs) -lGL -lGLEW -lSDL2_image -O3 -m64

.PHONY: all

default:
	$(MAKE) -j8 all

all: $(OUTDIR)/box16

# $(NFD_OBJDIR)/nfd.a: $(NFD_OBJS)
# 	ar $(NFD_ARFLAGS) $@ $^

$(NFD_OBJDIR)/%.o: $(NFD_SRCDIR)/%.c | $(NFD_OBJDIR)
	g++ $(NFD_CFLAGS) -c $< -o $@

$(NFD_OBJDIR):
	mkdir -p $@

$(OUTDIR)/box16: $(NFD_OBJS) $(BOX16_OBJS)
	mkdir -p $(OUTDIR)
	g++ $(BOX16_LDFLAGS) $(NFD_LDFLAGS) $^ -o $@
	cp $(REPODIR)/resources/*.png $(OUTDIR)/
	cp -r $(REPODIR)/resources/r39/* $(OUTDIR)/

$(BOX16_OBJDIR)/%.o: $(BOX16_SRCDIR)/%.cpp | $(BOX16_OBJDIR) $(BOX16_OBJDIR)/compat $(BOX16_OBJDIR)/cpu $(BOX16_OBJDIR)/gif $(BOX16_OBJDIR)/imgui $(BOX16_OBJDIR)/overlay $(BOX16_OBJDIR)/vera $(BOX16_OBJDIR)/ym2151
	g++ $(BOX16_CFLAGS) -c $< -o $@

$(BOX16_OBJDIR) $(BOX16_OBJDIR)/compat $(BOX16_OBJDIR)/cpu $(BOX16_OBJDIR)/gif $(BOX16_OBJDIR)/imgui $(BOX16_OBJDIR)/overlay $(BOX16_OBJDIR)/vera $(BOX16_OBJDIR)/ym2151:
	mkdir -p $@

clean:
	rm -rf obj