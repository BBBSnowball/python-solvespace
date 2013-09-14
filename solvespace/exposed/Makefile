WIN_DEFINES = -D_WIN32_WINNT=0x500 -D_WIN32_IE=0x500 -DWIN32_LEAN_AND_MEAN
DEFINES = -DISOLATION_AWARE_ENABLED -DLIBRARY
# Use the multi-threaded static libc because libpng and zlib do; not sure if anything bad
# happens if those mix, but don't want to risk it.
#TODO -MT - multithread?
#TODO /Zi /EHs /O2 /GS-
CFLAGS  = -Iextlib -I../../common/win32 -D_DEBUG -D_CRT_SECURE_NO_WARNINGS -I. -I.. -O2 -g -Wno-write-strings -fpermissive
CFLAGS_SHARED = -fPIC -shared $(CFLAGS)

HEADERS = ../solvespace.h ../dsc.h ../sketch.h ../expr.h slvs.h

OBJDIR = obj

SSOBJS   = $(OBJDIR)/util.obj \
		   $(OBJDIR)/entity.obj \
		   $(OBJDIR)/expr.obj \
		   $(OBJDIR)/constrainteq.obj \
		   $(OBJDIR)/system.obj \


W32OBJS  = $(OBJDIR)/w32util.obj \


LIBOBJS  = $(OBJDIR)/lib.obj \


#LIBS = user32.lib gdi32.lib comctl32.lib advapi32.lib shell32.lib
LIBS = 

CC = gcc
CXX = g++

all: cdemo
	LD_LIBRARY_PATH=. ./cdemo

clean:
	rm -f obj/* cdemo libslvs.so

.SECONDEXPANSION:

libslvs.so: $(SSOBJS) $(LIBOBJS) $(W32OBJS)
	$(CXX) -shared -fPIC -o$@ $(SSOBJS) $(LIBOBJS) $(W32OBJS) $(LIBS)

cdemo: CDemo.c libslvs.so
	$(CXX) $(CFLAGS) -o$@ CDemo.c -L. -lslvs $(LIBS)

$(SSOBJS): ../$$(basename $$(notdir $$@)).cpp $(HEADERS)
	$(CXX) $(CFLAGS_SHARED) $(DEFINES) -c -o$@ $<

$(W32OBJS): ../win32/$$(basename $$(notdir $$@)).cpp $(HEADERS)
	$(CXX) $(CFLAGS_SHARED) $(DEFINES) -c -o$@ $<

$(LIBOBJS): $$(basename $$(notdir $$@)).cpp $(HEADERS)
	$(CXX) $(CFLAGS_SHARED) $(DEFINES) -c -o$@ $<
