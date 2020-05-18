ODIR=obj
SDIR=src
BDIR=bin
TDIR=t
LDIR=logs
CXX=g++
CC=gcc
CFLAGS=-Wall -fexceptions -Inginx/src -Inginx/obj -O3 -fPIC -s
CXXFLAGS=$(CFLAGS) -std=c++17
LDFLAGS := $(LDFLAGS) -lxapian -s
AR=ar
SOURCES=$(wildcard $(SDIR)/*.cpp) $(wildcard $(SDIR)/*.c) $(wildcard $(TDIR)/*.cpp)
LIBRARYSOURCES=$(SDIR)/ngx_xapian_search.cpp
TESTSOURCES=$(wildcard $(TDIR)/*.cpp)
OBJECTS=$(patsubst %.cpp,%.o,$(patsubst %.c,%.o,$(patsubst $(SDIR)/%,$(ODIR)/%,$(SOURCES))))
LIBRARYOBJECTS=$(patsubst %.cpp,%.o,$(patsubst %.c,%.o,$(patsubst $(SDIR)/%,$(ODIR)/%,$(LIBRARYSOURCES))))
TESTOBJECTS=$(patsubst %.cpp,%.o,$(patsubst %.c,%.o,$(patsubst $(SDIR)/%,$(ODIR)/%,$(TESTSOURCES))))

TEST = $(BDIR)/test
LIBRARY=$(BDIR)/libnginx_xapian.a
NGINX_LIBRARY=$(BDIR)/ngx_xapian_search_module.so


test: $(LIBRARYOBJECTS) $(TESTOBJECTS)
	$(CXX) $(LIBRARYOBJECTS) $(TESTOBJECTS) -o $(TEST) $(LDFLAGS) -lgtest -lpthread

all: $(LIBRARY)

$(LIBRARY): directories $(LIBRARYOBJECTS)
	$(AR) -r -s $(LIBRARY) $(LIBRARYOBJECTS)

$(NGINX_LIBRARY): $(LIBRARYOBJECTS)
	$(CC) $(LIBRARYOBJECTS) -shared

$(ODIR)/%.o: $(SDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(ODIR)/%.o: $(TDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(ODIR)/%.o: $(SDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@


library: $(LIBRARY)

directories: $(ODIR) $(BDIR) $(LDIR) $(TDIR)

$(LDIR):
	mkdir -p $(LDIR)
$(ODIR):
	mkdir -p $(ODIR)
$(BDIR):
	mkdir -p $(BDIR)

clean: directories
	rm -f $(ODIR)/*.o $(LIBRARY) $(TEST)

cleantest: clean
