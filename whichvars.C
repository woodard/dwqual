#include <string>
#include <vector>
#include <set>
#include <iostream>
#include <algorithm>
#include <ios>

#include <dyninst/Symtab.h>
#include <dyninst/Function.h>
#include <dyninst/Variable.h>
#include <dyninst/Type.h>

#include <boost/icl/interval_map.hpp>

#include <unistd.h>

using namespace Dyninst;
using namespace SymtabAPI;
using namespace std;
using namespace boost;
using namespace boost::icl;

enum exit_codes {
		 EXIT_OK=0,
		 EXIT_ARGS=1,
		 EXIT_MODULE=2,
		 EXIT_NOFUNCS=3,
		 EXIT_LFUNC=4,
		 EXIT_LF_NOTUNIQ=5,
		 EXIT_GLOBALS=6
};

int main(int argc, char **argv){
  //Name the object file to be parsed:
  std::string file;
  int opt;
  bool verbose=false;
  
  while ((opt = getopt(argc, argv, "v")) != -1) {
    switch (opt) {
    case 'v':
      verbose=true;
      break;
    default:
      std::cerr << "Usage:" << argv[0] << " [-g][-f][-l] name" << std::endl;
      exit(EXIT_ARGS);
    }
  }

  if(optind >= argc)
    file= "./raja-perf.exe";
  else
    file=argv[optind];

  Symtab *obj = NULL;
  // Parse the object file
  bool err = Symtab::openFile(obj, file);

  if( err == false)
    exit(EXIT_MODULE);

  /*--------*/
  typedef set<localVar*> LVarSet;
  interval_map<Address, LVarSet> llmap;
  //iterate through all the functions
  vector <Function *> funcs;
  if (!obj->getAllFunctions(funcs))
    exit(EXIT_NOFUNCS);
  for(auto i=funcs.begin();i!=funcs.end();i++){
    if(verbose)
      cout << endl << (*i)->getName() << endl;
    //iterate through all the local variables and parameters
    vector <localVar *> lvars;
    (*i)->getParams(lvars);
    (*i)->getLocalVariables(lvars);
    for(auto j=lvars.begin();j!=lvars.end();j++){
      if(verbose)
	cout << '\t' << (*j)->getName() << endl;
      vector<VariableLocation> &lvlocs=(*j)->getLocationLists();
      for(auto k=lvlocs.begin();k!=lvlocs.end();k++){
	discrete_interval<Address> addr_inter
	  = construct<discrete_interval<Address> >
	  ((*k).lowPC,(*k).hiPC,interval_bounds::closed());
	if(verbose)
	  cout << "\t\t" << addr_inter << endl;
	LVarSet newone;
	newone.insert(*j);
	llmap.add(make_pair(addr_inter,newone));
      }
    }
  }

  for(auto i=llmap.begin();i!=llmap.end();i++){
    cout << i->first << ": " << endl;
    for( auto j=i->second.begin();j!=i->second.end();j++)
      if( (*j)->getName()=="this")
	cout << "\tthis <" << (*j)->getType()->getName() << ">\n";
      else
	cout << '\t' << (*j)->getName() << " [" << (*j)->getFileName() << ':'
	     << (*j)->getLineNum() << "]\n";
    cout << endl;
  }
}
