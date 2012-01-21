CC=gcc
CXX=g++
CFLAGS+=-DGCC
CXXFLAGS += $(CFLAGS)


NAME=propcomp
TARGET=PropCom
MAIN=$(TARGET)/$(TARGET)
MAINOBJ=$(MAIN).o
MAINSRC=$(MAIN).cpp
SRCDIR=PropellerCompiler
OBJ=$(SRCDIR)/BlockNestStackRoutines.o \
    $(SRCDIR)/CompileDatBlocks.o \
    $(SRCDIR)/CompileExpression.o \
    $(SRCDIR)/CompileInstruction.o \
    $(SRCDIR)/CompileUtilities.o \
    $(SRCDIR)/DistillObjects.o \
    $(SRCDIR)/Elementizer.o \
    $(SRCDIR)/ErrorStrings.o \
    $(SRCDIR)/ExpressionResolver.o \
    $(SRCDIR)/InstructionBlockCompiler.o \
    $(SRCDIR)/StringConstantRoutines.o \
    $(SRCDIR)/SymbolEngine.o \
    $(SRCDIR)/Utilities.o \
    $(SRCDIR)/PropellerCompiler.o \


all: $(OBJ) Makefile
	$(CXX) -o $(NAME) $(CFLAGS) $(MAINSRC) $(OBJ)


%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<


clean:
	rm -rf $(SRCDIR)/*.o $(MAINOBJ) $(NAME)
