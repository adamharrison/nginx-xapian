ODIR=obj
SDIR=src
BDIR=bin
TDIR=t
LDIR=logs
CPPC=g++
CC=gcc
CFLAGS=-Wall -fexceptions -Inginx/src -Inginx/obj
EXTRAFLAGS=-O3 -fPIC
LDFLAGS := $(LDFLAGS) -flto -s -static-libstdc++ -static-libgcc
AR=ar
SOURCES=$(wildcard $(SDIR)/*.cpp) $(wildcard $(SDIR)/*.c)
OBJECTS=$(patsubst %.cpp,%.o,$(patsubst %.c,%.o,$(patsubst $(SDIR)/%,$(ODIR)/%,$(SOURCES))))

BINARY=$(BDIR)/ngx_xapian_search_module.so


all: $(BINARY) 

$(BINARY): directories $(LIBOBJECTS)
	$(CC) -o $(LIBRARY) $(LIBOBJECTS) -shared

$(ODIR)/%.o: $(SDIR)/%.cpp
	$(CPPC) -std=c++11 $(CFLAGS) $(EXTRAFLAGS) -c $< -o $@

$(ODIR)/%.o: $(SDIR)/%.c
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -c $< -o $@

binary: $(BINARY)

directories: $(ODIR) $(BDIR) $(LDIR)

$(LDIR):
	mkdir -p $(LDIR)
$(ODIR):
	mkdir -p $(ODIR)
$(BDIR):
	mkdir -p $(BDIR)

clean: directories
	rm -f $(ODIR)/*.o $(BINARY)
