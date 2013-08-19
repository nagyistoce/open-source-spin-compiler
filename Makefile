CC=gcc
CXX=g++
CFLAGS+=-DGCC -Wall -g -static


NAME=openspin
TARGET=openspin
MAIN=SpinSource/$(TARGET)
FLEXBUF=SpinSource/flexbuf
PREPROC=SpinSource/preprocess
MAINOBJ=$(MAIN).o $(FLEXBUF).o $(PREPROC).o
MAINSRC=$(MAIN).cpp $(FLEXBUF).c $(PREPROC).c
LIBDIR=PropellerCompiler
LIBNAME=$(LIBDIR)/libopenspin.a


all: $(LIBNAME) Makefile
	$(CXX) -o $(NAME) $(CFLAGS) $(MAINSRC) $(LIBNAME)


$(LIBNAME):
	make -C $(LIBDIR) all


clean:
	rm -rf $(MAINOBJ) $(LIBNAME)
	make -C $(LIBDIR) clean
