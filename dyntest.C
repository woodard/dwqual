#include <string>
#include <vector>
#include <set>
#include <iostream>
#include <algorithm>
#include <ios>

#include <stdlib.h>
#include <unistd.h>

#include <dyninst/Symtab.h>
#include <dyninst/Function.h>
#include <dyninst/Variable.h>
#include <dyninst/Type.h>

using namespace Dyninst;
using namespace SymtabAPI;

static const unsigned CACHELINE_BITS=6; // assumption cache lines are 64 bytes
// The initial size of the vector with cache lines in it.
static const unsigned CLINE_SIZE=100;
enum exit_codes {
		 EXIT_OK=0,
		 EXIT_ARGS=1,
		 EXIT_MODULE=2,
		 EXIT_NOFUNCS=3,
		 EXIT_LFUNC=4,
		 EXIT_LF_NOTUNIQ=5,
		 EXIT_GLOBALS=6
};

static void print_type( Type *t);
static void print_loclists( std::vector<VariableLocation> &ll);

static void do_dump_globals(Symtab *obj);
static void do_dump_functions(Symtab *obj);
static void do_dump_locals(Symtab *obj, char *loc_fname);
static void do_dump_types(Symtab *obj);
static void do_pahole(Symtab *obj);

int main(int argc, char **argv){
  //Name the object file to be parsed:
  std::string file;
  int opt;
  bool dump_globals=false;
  bool dump_functions=false;
  bool dump_locals=false;
  bool dump_types=false;
  bool pahole=false;
  char *loc_fname;
  
  while ((opt = getopt(argc, argv, "fgl:tp")) != -1) {
    switch (opt) {
    case 'f':
      dump_functions=true;
      break;
    case 'g':
      dump_globals=true;
      break;
    case 'l':
      dump_locals=true;
      loc_fname=optarg;
      break;
    case 'p':
      pahole=true;
      break;
    case 't':
      dump_types=true;
      break;

    default: /* '?' */
      std::cerr << "Usage:" << argv[0] << " [-g][-f][-l] name" << std::endl;
      exit(EXIT_ARGS);
    }
  }

  if(optind >= argc)
    file= "./raja-perf.exe";
  else
    file=argv[optind];
  

  //Declare a pointer to an object of type Symtab; this represents the file.
  Symtab *obj = NULL;
  // Parse the object file
  bool err = Symtab::openFile(obj, file);

  if( err == false)
    exit(EXIT_MODULE);

  if(dump_globals)
    do_dump_globals(obj);

  // It looks like BSS variables are present but they are of unknown type.
  // https://github.com/dyninst/dyninst/issues/619
  
  // std::vector <Symbol *> syms;
  // if (obj->findSymbol(syms, "__bss_start", Symbol::ST_UNKNOWN)) {
  //   std::cout << std::endl << syms[0]->getType() << std::endl;
  // } else
  //   std::cout << "elsewhere" << std::endl;
  // but all the BSS variables don't appear in the list of variable.s
  // vars.clear();
  // if (obj->findVariablesByName(vars, "__bss_start")) {
  //   std::cout << std::endl << vars[0] << std::endl;
  // }

  //  std::for_each(vars.begin(),vars.end(),print_vars);
  // std::cout << vars[0]->getType() << ' ' << vars[0]->getOffset() << ' ' << *vars[0] << std::endl;
  
  if(dump_functions)
    do_dump_functions(obj);
  
  if(dump_locals)
    do_dump_locals(obj, loc_fname);

  if(dump_types)
    do_dump_types(obj);

  if(pahole)
    do_pahole(obj);

  exit(EXIT_OK);
}

/*----------------------------------------------------*/
// This needs to be recursive somehow
void print_type( Type *t){
  switch(t->getDataClass()){
  case dataTypedef:
    do{
      std::cout << "\n\ttypedef of "
		<< t->getTypedefType()->getConstituentType()->getName() << ' ';
      t=t->getTypedefType()->getConstituentType();
    }while(t->getDataClass()==dataTypedef);
    print_type(t);
    break;
  case dataArray:
    {
      typeArray *ta=t->getArrayType();
      std::cout << "\n\tarray of " << ta->getBaseType()->getName()
		<< "[" << ta->getLow() << ".." << ta->getHigh() << "]\n";
    }
    break;
  case dataStructure:
    {
      auto fields=t->getStructType()->getComponents();      
      std::cout << "Structure with " << fields->size() << " components\n";
      std::for_each(fields->begin(),fields->end(),
		   [](const auto &p){
		     std::cout << "\t\t" << p->getName() << ' '
			       << p->getOffset() << std::endl;
		     print_type(p->getType());});
    }
    break;
  case dataPointer:
    std::cout << "Pointer to "
	      << t->getPointerType()->getConstituentType()->getName();
    print_type( t->getPointerType()->getConstituentType());
    break;
  case dataFunction:
    std::cout << "Function Pointer";
    break; 
  default:
    std::cout << t->getDataClass();
  }
  //  std::cout << ' ' << t->getSize() << 'b' << std::endl;
  std::cout << std::endl;
}

void print_loclists( std::vector<VariableLocation> &ll){
  std::cout << "\tLocation Lists" << std::endl;
  std::for_each(ll.begin(),ll.end(),
		[](const auto &p){
		  std::cout << "\t\t" << std::hex << p.lowPC
			    << ' ' << p.hiPC << std::dec;
		  const char *sclass;
		  switch(p.stClass){
		  case storageUnset:
		    sclass="Unset";
		    break;
		  case storageAddr:
		    sclass="Addr";
		    break;
		  case storageReg:
		    sclass="Register";
		    break;
		  case storageRegOffset:
		    sclass="RegOffset";
		    break;
		  default:
		    exit(6);
		  }
		  // XXX handle unset properly
		  const char *indirect=p.refClass==storageRef?"*":"";
		  std::cout << ' ' << sclass << ' ';
		  switch(p.stClass){
		  case storageUnset:
		    sclass="Unset";
		    break;
		  case storageAddr:
		    std::cout << indirect << p.frameOffset;
		    break;
		  case storageReg:
		    std::cout << indirect << p.mr_reg;
		    break;
		  case storageRegOffset:
		    std::cout << indirect << '[' << "fp" << (p.frameOffset>0?"+":"") << p.frameOffset << ']';
		    break;
		  default:
		    exit(6);
		  }
		  std::cout << std::endl;});
  std::cout << std::endl;				      
}

void do_dump_globals(Symtab *obj){
  std::vector <Variable *> vars;
  if (!obj->getAllVariables(vars))
    exit(EXIT_GLOBALS);

  auto sort_vars_offset = []( const auto &lhs, const auto &rhs){
			    return lhs->getOffset() < rhs->getOffset(); };
  std::sort(vars.begin(),vars.end(),sort_vars_offset);

  unsigned line=0;
  std::vector< std::vector< Variable *> > clines;
  clines.resize(100);
  auto lastone=vars.begin();
  clines[0].push_back(*lastone);
  for( auto i=vars.begin();i!=vars.end();i++){
    if(i==vars.begin()){
      // std::cout << "Cacheline " << line << std::endl << '\t' << *vars[0]
      // 		<< std::endl;
      continue;
    }

    if( (*lastone)->getOffset() >> CACHELINE_BITS !=
	(*i)->getOffset() >> CACHELINE_BITS){
      if(clines[line].size()==1)
	clines[line].clear(); // this is an uncontested line
      else{
	line++;
	// std::cout << "Cacheline " << line << std::endl;
      }
    }
    clines[line].push_back(*i);
    // std::cout << '\t' << (*i)->getOffset() << ' ' << (*i)->getSize() << ' '
    // 	      << **i << std::endl;
    lastone=i;
  }
  
  auto print_vars = [](const auto& p){
		      std::for_each(p->pretty_names_begin(),
				    p->pretty_names_end(),
				    [](const auto &a){
				      std::cout << a << ' ';});
		      std::cout << ": " << std::hex << p->getOffset()
				<< std::dec << ' ' << p->getSize() << "b ";
		      if(p->getType()){
			std::cout << "\n\tType:";
			print_type(p->getType());
		      }
		      // std::cout << p->getType()->getName();		 
		      // else
		      // 	std::cout << *p;
		      std::cout << std::endl; };
  std::for_each(clines.begin(), clines.end(),
		[&print_vars] (const auto& v){
		  if(!v.empty()){
		    std::cout << std::endl;
		    std::for_each(v.begin(),v.end(), print_vars);
		  }
		});
}

void do_dump_functions(Symtab *obj){
  std::vector <Function *> funcs;
  if (!obj->getAllFunctions(funcs))
    exit(EXIT_NOFUNCS);
  auto print_funcs = [](const auto& p){
		       std::cout << *(p->typed_names_begin()) << '\t'
				 << p->getName() << std::endl; };
  std::for_each(funcs.begin(),funcs.end(),print_funcs);
  
}

void do_dump_locals(Symtab *obj, char *loc_fname){
  std::vector <Function *> funcs;
  if (!obj->findFunctionsByName(funcs,loc_fname))
    exit(EXIT_LFUNC);
  if(funcs.size()!=1)
    exit(EXIT_LF_NOTUNIQ);
  std::vector <localVar *> lvars;
  (*funcs.begin())->getParams(lvars);
  auto num_params=lvars.size();
  std::cout << "Number of params: " << num_params << std::endl;

  // dyninst is asserting on this
  if(lvars[0]->getType()->getDataClass()==dataTypedef){
    auto t=lvars[0]->getType()->getTypedefType()->getConstituentType();
    std::cout << t->getName() << t->getDataClass() << std::endl;
    auto p=t->getPointerType()->getConstituentType();
    std::cout << p->getName() << (p->getDataClass()==dataStructure)
	      << std::endl;
    auto s=p->getStructType();
    std::cout << s << ' ' << s->getName() << std::endl;
    auto vt=s->getComponents();
  } 
      
  // (*funcs.begin())->getLocalVariables(lvars);
  // std::cout << "Number of local_vars: " << lvars.size()-num_params
  // 	      << std::endl;
  // std::for_each(lvars.begin(),lvars.end(),
  // 		  [](const auto &p){
  // 		    Type *t=p->getType();
  // 		    std::cout << p->getName() << '\t' << t->getName() << ' ';
  // 		    print_type(t);
  // 		    print_loclists(p->getLocationLists());}
  // 		  );  
}

void map_type_to_vars(Symtab *obj, std::multimap< Type*, localVar*> &map){
  std::vector <Function *> funcs;
  if (!obj->getAllFunctions(funcs))
    exit(EXIT_NOFUNCS);
  for( auto i=funcs.begin();i!=funcs.end();i++){
    std::vector <localVar *> lvars;
    (*i)->getLocalVariables(lvars);
    std::for_each(lvars.begin(),lvars.end(),
		  [&map](const auto &p){map.insert(std::make_pair(p->getType(),p));});
  }
}

static void insert_types( std::set <Type *> &types, Type *p){
  // Don't add duplicate, you'll ininitely recures.
  if(p==NULL || types.find(p)!=types.end())
    return;
  // std::cout << p->getName();
  types.insert(p);
  switch(p->getDataClass()){
  case dataPointer:
    insert_types(types, p->getPointerType()->getConstituentType());
    break;
  case dataTypedef:
    insert_types(types, p->getTypedefType()->getConstituentType());
    break;
  case dataReference:
    insert_types(types, p->getRefType()->getConstituentType());
    break;
  case dataArray:
    insert_types(types, p->getArrayType()->getBaseType());
    break;
  case dataStructure:
    {
      auto members=p->getStructType()->getComponents();
      std::for_each(members->begin(),members->end(),
		    [&types](const auto &mem){
		      insert_types(types,mem->getType());});
    }
    break;
  }
  // std::cout << " done" << std::endl;
}

static void build_type_list(Symtab *obj, std::set <Type *> &types){
    obj->parseTypesNow();
  // these types are the general types used by fortran and they are not the
  // structures needed for pahole kind of functionality
  auto stypes=obj->getAllstdTypes();
  auto btypes=obj->getAllbuiltInTypes();
  std::for_each(stypes->begin(),stypes->end(),
		[&types](const auto &p){insert_types(types,p);});
  std::for_each(btypes->begin(),btypes->end(),
		[&types](const auto &p){insert_types(types,p);});

  // std::cerr << types.size() << ' ' << "vars\n";
  std::vector <Variable *> vars;
  if (!obj->getAllVariables(vars))
    exit(EXIT_GLOBALS);
  std::for_each(vars.begin(),vars.end(),
		[&types](const auto &p){
		  if(p->getType())
		    insert_types(types,p->getType());});

  std::vector <Function *> funcs;
  if (!obj->getAllFunctions(funcs))
    exit(EXIT_NOFUNCS);
  auto accumulate_types=
    [&types](const auto &fn){
      std::vector <localVar *> lvars;
      /* Even though the parameters are also local variables which may have 
	 types don't inspect them and assume that the types being passed into
	 them will be referenced elsewhere. This avoids a segv in dyninst when
	 inspecting a this pointer */
      // fn->getParams(lvars);
      fn->getLocalVariables(lvars);
      std::for_each(lvars.begin(),lvars.end(),
		    [&types](const auto &p){
		      if(p->getType())
			insert_types(types,p->getType());});
    };
  std::for_each(funcs.begin(),funcs.end(),accumulate_types);
}

void do_dump_types(Symtab *obj){
  std::set <Type *> types;
  build_type_list(obj, types);
  std::for_each(types.begin(),types.end(),
		[](const auto &p){
		  std::cout << p->getName() << ' ' << p->getSize()
			    << std::endl;});
}

void do_pahole(Symtab *obj){
  std::set <Type *> types;
  build_type_list(obj, types);
  std::cout << types.size() << " types\n";
  // discard anything that isn't a structure
  // Note: this is the proper way to delete elements from a set while iterating
  // through it.
  for( auto i=types.begin();i!=types.end(); ){
    if( (*i)->getDataClass()!=dataStructure ||
	// or is too short to be interesting
	(*i)->getSize() < (1<<CACHELINE_BITS) ){ 
      i=types.erase(i);
      continue;
    }
    auto members=(*i)->getStructType()->getComponents();
    std::cout << "class " << (*i)->getName() << ' ' << " {" << std::endl
	      << "  // Size: " << (*i)->getSize() << ", Cachelines: "
	      << ((*i)->getSize() >> CACHELINE_BITS)+1 << ", Members: "
	      << members->size() << std::endl << std::endl;

    
    std::for_each(members->begin(),members->end(),
		  [&types](const auto &mem){
		    // -1s are virtual functions ignore them for the moment
		    if(mem->getOffset()!=-1)
		      std::cout << "  " << mem->getType()->getName() << "\t\t"
				<< mem->getName() << "; // " << mem->getOffset()
				<< ' ' << mem->getType()->getSize()
				<< std::endl;});
    std::cout << "};" << std::endl << std::endl;
    ++i;
  }
  // std::cout << types.size() << " structures\n";
}
