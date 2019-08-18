CXXFLAGS=-g -O2 -flto -fvar-tracking-assignments -gstatement-frontiers -gvariable-location-views

dyntest: dyntest.o
	c++ $(CXXFLAGS) -o dyntest dyntest.o -L /usr/lib64/dyninst -l symtabAPI -l tbb -l common

dyntest.o: dyntest.C

clean:
	rm -f *.o dyntest dyntest-clang *~

dyntest-clang.o: dyntest.C
	clang++ -g -O2 -c dyntest.C -o dyntest-clang.o

dyntest-clang: dyntest-clang.o
	clang++ -g -O2 -o dyntest-clang dyntest-clang.o -L /usr/lib64/dyninst -l symtabAPI -ltbb -lcommon
