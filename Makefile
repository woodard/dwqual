GCCXXFLAGS=-fvar-tracking-assignments -gstatement-frontiers -gvariable-location-views
CXXFLAGS=-g

all: dyntest whichvars.O2 whichvars.lto whichvars.O0 whichvars.O1 whichvars.O3 whichvars.Og whichvars.clang

dyntest: dyntest.o
	c++ $(CXXFLAGS) $(GCCXXFLAGS) -o dyntest dyntest.o -L /usr/lib64/dyninst -l symtabAPI -l tbb -l common

dyntest.o: dyntest.C

whichvars.O0: whichvars.O0.o
	c++  $(CXXFLAGS) $(GCCXXFLAGS) -O0 -o whichvars.O0 whichvars.O0.o -L /usr/lib64/dyninst -l symtabAPI -l tbb -l common

whichvars.O1: whichvars.O1.o
	c++ $(CXXFLAGS) $(GCCXXFLAGS) -O1 -o whichvars.O1 whichvars.O1.o -L /usr/lib64/dyninst -l symtabAPI -l tbb -l common

whichvars.O2: whichvars.O2.o
	c++ $(CXXFLAGS) $(GCCXXFLAGS) -O2 -o whichvars.O2 whichvars.O2.o -L /usr/lib64/dyninst -l symtabAPI -l tbb -l common

whichvars.O3: whichvars.O3.o
	c++ $(CXXFLAGS) $(GCCXXFLAGS) -O3 -o whichvars.O3 whichvars.O3.o -L /usr/lib64/dyninst -l symtabAPI -l tbb -l common

whichvars.Og: whichvars.Og.o
	c++ $(CXXFLAGS) $(GCCXXFLAGS) -Og -o whichvars.Og whichvars.Og.o -L /usr/lib64/dyninst -l symtabAPI -l tbb -l common

whichvars.lto: whichvars.lto.o
	c++ $(CXXFLAGS) $(GCCXXFLAGS) -O2 -flto -o whichvars.lto whichvars.lto.o -L /usr/lib64/dyninst -l symtabAPI -l tbb -l common

whichvars.clang: whichvars.clang.o
	clang++ $(CXXFLAGS) -O2 -flto -o whichvars.clang whichvars.clang.o -L /usr/lib64/dyninst -l symtabAPI -l tbb -l common

whichvars.O0.o: whichvars.C
	c++ -O0 $(CXXFLAGS) $(GCCXXFLAGS) -c -o whichvars.O0.o whichvars.C

whichvars.O1.o: whichvars.C
	c++ -O1 $(CXXFLAGS) $(GCCXXFLAGS) -c -o whichvars.O1.o whichvars.C

whichvars.O2.o: whichvars.C
	c++ -O2 $(CXXFLAGS) $(GCCXXFLAGS) -c -o whichvars.O2.o whichvars.C

whichvars.O3.o: whichvars.C
	c++ -O3 $(CXXFLAGS) $(GCCXXFLAGS) -c -o whichvars.O3.o whichvars.C

whichvars.Og.o: whichvars.C
	c++ -Og $(CXXFLAGS) $(GCCXXFLAGS) -c -o whichvars.Og.o whichvars.C

whichvars.lto.o: whichvars.C
	c++ -O2 -flto $(CXXFLAGS) $(GCCXXFLAGS) -c -o whichvars.lto.o whichvars.C

whichvars.clang.o: whichvars.C
	clang++ -O2 -flto $(CXXFLAGS) -c -o whichvars.clang.o whichvars.C

clean:
	rm -f *.o dyntest dyntest-clang whichvars whichvars.o *~

dyntest-clang.o: dyntest.C
	clang++ -g -O2 -c dyntest.C -o dyntest-clang.o

dyntest-clang: dyntest-clang.o
	clang++ -g -O2 -o dyntest-clang dyntest-clang.o -L /usr/lib64/dyninst -l symtabAPI -ltbb -lcommon
