CC=gcc
CXX=g++
CFLAGS+=-DGCC


NAME=spin
TARGET=spin
MAIN=SpinSource/$(TARGET)
MAINOBJ=$(MAIN).o
MAINSRC=$(MAIN).cpp
LIBDIR=PropellerCompiler
LIBNAME=$(LIBDIR)/libspin.a


all: $(LIBNAME) Makefile
	$(CXX) -o $(NAME) $(CFLAGS) $(MAINSRC) $(LIBNAME)


$(LIBNAME):
	make -C $(LIBDIR) all


clean:
	rm -rf $(MAINOBJ) $(LIBNAME)
	make -C $(LIBDIR) clean
