ODIR=obj
SDIR=src
BDIR=bin
TDIR=t
LDIR=logs
CXX=g++
CC=gcc
CFLAGS=-Wall -fexceptions -Inginx/src -Inginx/obj -O2 -fPIC
CXXFLAGS=$(CFLAGS) -std=c++17
LDFLAGS := $(LDFLAGS) -flto -s -static-libstdc++ -static-libgcc
AR=ar
SOURCES=$(wildcard $(SDIR)/*.cpp) $(wildcard $(SDIR)/*.c)
LIBRARYSOURCES=$(wildcard $(SDIR)/*.cpp)
OBJECTS=$(patsubst %.cpp,%.o,$(patsubst %.c,%.o,$(patsubst $(SDIR)/%,$(ODIR)/%,$(SOURCES))))
LIBRARYOBJECTS=$(patsubst %.cpp,%.o,$(patsubst %.c,%.o,$(patsubst $(SDIR)/%,$(ODIR)/%,$(LIBRARYSOURCES))))

LIBRARY=$(BDIR)/libnginx_xapian.a

NGINX_LIBRARY=$(BDIR)/ngx_xapian_search_module.so


all: $(LIBRARY)

$(LIBRARY): directories $(LIBRARYOBJECTS)
	$(AR) -r -s $(LIBRARY) $(LIBRARYOBJECTS)

$(ODIR)/%.o: $(SDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(ODIR)/%.o: $(SDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@


library: $(LIBRARY)

directories: $(ODIR) $(BDIR) $(LDIR)

$(LDIR):
	mkdir -p $(LDIR)
$(ODIR):
	mkdir -p $(ODIR)
$(BDIR):
	mkdir -p $(BDIR)

clean: directories
	rm -f $(ODIR)/*.o $(LIBRARY)
