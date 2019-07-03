#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <ios>

#include <stdlib.h>

#include <dyninst/Symtab.h>
#include <dyninst/Function.h>
#include <dyninst/Variable.h>
#include <dyninst/Type.h>

using namespace Dyninst;
using namespace SymtabAPI;

static const unsigned CACHELINE_BITS=6;

int main(int argc, char **argv){
  //Name the object file to be parsed:
  std::string file;
  if(argc==1)
    file= "./raja-perf.exe";
  else
    file=argv[1];
  

  //Declare a pointer to an object of type Symtab; this represents the file.
  Symtab *obj = NULL;
  // Parse the object file
  bool err = Symtab::openFile(obj, file);

  if( err == false)
    exit(1);

  std::vector <Variable *> vars;
  if (!obj->getAllVariables(vars))
    exit(3);
  auto print_vars = [](const auto& p){
		      std::cout << p->getModule()->fullName() << ": " << std::hex
				<< p->getOffset() << std::dec 
				<< ' ' << p->getSize() << "b ";
		      if(p->getType())
			std::cout << p->getType()->getName();
		      // else
		      // 	std::cout << *p;
		      std::cout << std::endl; };
  auto sort_vars_offset = []( const auto &lhs, const auto &rhs){
			    return lhs->getOffset() < rhs->getOffset(); };
  std::sort(vars.begin(),vars.end(),sort_vars_offset);

  unsigned line=1;
  auto lastone=vars.begin();
  for( auto i=vars.begin();i!=vars.end();i++){
    if(i==vars.begin()){
      std::cout << "Cacheline " << line << std::endl << '\t' << *vars[0]
		<< std::endl;
      continue;
    }
    // assumption cache lines are 64 bytes
    if( (*lastone)->getOffset() >> CACHELINE_BITS != (*i)->getOffset() >> CACHELINE_BITS){
      line++;
      std::cout << "Cacheline " << line << std::endl;
    }
    std::cout << '\t' << (*i)->getOffset() << ' ' << (*i)->getSize() << ' ' << **i << std::endl;
    lastone=i;
  }
  //  std::for_each(vars.begin(),vars.end(),print_vars);
  // std::cout << vars[0]->getType() << ' ' << vars[0]->getOffset() << ' ' << *vars[0] << std::endl;
  
  // std::vector <Function *> funcs;
  // if (!obj->getAllFunctions(funcs))
  //   exit(2);
  // auto print_funcs = [](const auto& p){
  // 		 std::cout << p->getName() << std::endl; };
  // std::for_each(funcs.begin(),funcs.end(),print_funcs);

  exit(0);
}
