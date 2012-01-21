CC=gcc
CXX=g++
CFLAGS+=-DGCC


NAME=propcomp
TARGET=PropCom
MAIN=$(TARGET)/$(TARGET)
MAINOBJ=$(MAIN).o
MAINSRC=$(MAIN).cpp
LIBDIR=PropellerCompiler
LIBNAME=$(LIBDIR)/libpropcomp.a


all: $(LIBNAME) Makefile
	$(CXX) -o $(NAME) $(CFLAGS) $(MAINSRC) $(LIBNAME)


$(LIBNAME):
	make -C $(LIBDIR) all


clean:
	rm -rf $(MAINOBJ) $(LIBNAME)
	make -C $(LIBDIR) clean
