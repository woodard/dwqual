CXXFLAGS=-g -O2 -flto

dyntest: dyntest.o
	c++ -flto -g -o dyntest dyntest.o -L /usr/lib64/dyninst -l symtabAPI -l tbb -l common

dyntest.o: dyntest.C

clean:
	rm *.o dyntest *~
