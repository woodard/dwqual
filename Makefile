CXXFLAGS=-g -O2

dyntest: dyntest.o
	c++ -g -o dyntest dyntest.o -L /usr/lib64/dyninst -l symtabAPI

dyntest.o: dyntest.C
