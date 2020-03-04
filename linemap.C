#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <iostream>
#include <algorithm>
#include <ios>

#include <dyninst/Symtab.h>
#include <dyninst/Function.h>
#include <dyninst/Variable.h>
#include <dyninst/Type.h>

#include <boost/icl/interval_map.hpp>

#include <getopt.h>
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

ostream *errfile;

static void warn_line_range( const string &filename, const string &funcname,
		      const string &varname, int line, int size){
  *errfile << "DWARF Warning: " << funcname<< ':' << varname
       << " line number out of range: " << filename << ' ' << line << '/'
       << size << endl;
}

struct file_data{
  string file_name;
  bool inlined;
  file_data( const string &f,bool i=false):file_name(f),inlined(i){}
  file_data( const file_data &o){file_name=o.file_name;inlined=o.inlined;};
  bool operator<(const file_data &o) const { return file_name<o.file_name;}
  bool operator==(const file_data &o) const { return file_name<o.file_name;}
};

struct line_data{
  string line;
  set< localVar*> decls;
  set< localVar*> avail;
  line_data(string &l):line(l){}
  line_data(){};
};

static bool read_file(std::map< string, vector<line_data > > &file_lines,
		      const string &f){
  ifstream inf(f);
  if(inf.fail()){
    cerr << "Error: Problem reading source file " << f << ". Skipping:"
	 << strerror(errno) << endl;
    inf.close();
    return false;
  }
  file_lines.insert(pair<string,vector<line_data> >(f, vector<line_data>()));
  string line;
  while(getline(inf,line))
    file_lines[f].push_back(line_data(line));
  inf.close();
  return true;
}

static void usage( ostream &os, char *prog_name){
      os << "Usage:" << prog_name << " [-v][-w][-q][-m] name" << std::endl
	 << "\t-v | --verbose" << std::endl
	 << "\t-w | --warnings DWARF warnings-only" << std::endl
	 << "\t-m | --machine-readable" << std::endl
	 << "\t-q | --quiet" << std::endl;
}

int main(int argc, char **argv){
  //Name the object file to be parsed:
  std::string file;
  int opt;
  static struct option long_options[] =
    {
     {"verbose", no_argument, 0, 'v'},
     {"warnings", no_argument, 0, 'w'},
     {"machine-readable", no_argument, 0, 'm'},
     {"quiet", no_argument, 0, 'q'},
     {"help", no_argument, 0, '?'},
     {0, 0, 0, 0 }
    };
  int option_index = 0;

  bool verbose=false;
  bool quiet=false;
  bool machine=false;
  errfile=&cerr;
  
  while ((opt = getopt_long(argc, argv, "vwqm", long_options, &option_index)) != -1) {
    switch (opt) {
    case 'v':
      verbose=true;
      break;
    case 'w':
      errfile=&cout;
      quiet=true;
      break;
    case 'm':
      machine=true;
      break;
    case 'q':{
      quiet=true;
      errfile=new ofstream("/dev/null");
      break;
    }
    case '?':
      usage(std::cout, argv[0]);
      exit(EXIT_OK);
    default:
      usage(std::cerr, argv[0]);
      exit(EXIT_ARGS);
    }
  }

  // this is mostly for debugging convienence.
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
  typedef set< pair<localVar*,Function*> > LVarSet;
  interval_map<Address, LVarSet> llmap;
  //iterate through all the functions
  vector <Function *> funcs;
  if (!obj->getAllFunctions(funcs))
    exit(EXIT_NOFUNCS);

  set< file_data> files;
  for( auto i: funcs) {
    if(!i->getModule()->fullName().empty()){
       cerr << "f " << i->getName() << " Inserting: "
	    << i->getModule()->fullName() << endl;
      if( i->getModule()->language() == lang_Unknown){
	*errfile << "DWARF Warning: " << i->getModule()->fullName()
		 << " is of unknown type. Skipping.\n";
	continue;
      }
      files.insert(i->getModule()->fullName());
    } else
      *errfile << "DWARF Warning: Function " << i->getName()
	   << " has an empty filename in its module.\n";
    if(verbose)
      cout << endl << "Func: " << i->getName() << endl;
    //iterate through all the local variables and parameters
    vector <localVar *> lvars;
    i->getParams(lvars);
    i->getLocalVariables(lvars);
    for(auto j: lvars) {
      if(!j->getFileName().empty()){
	// *errfile << "v Inserting: " << j->getFileName() << endl;
	files.insert( j->getFileName());
      }else
	*errfile << "DWARF Warning: Variable " << i->getName() << ':'
		 << j->getName() << " has an empty filename.\n";
      if(verbose) {
	if( j->getName()=="this")
	  cout << "\tthis <" << j->getType()->getName() << ">\n";
	else
	  cout << '\t' << j->getName() << " Defined: " << j->getFileName()
	       << ':'  << j->getLineNum() << endl;
      }
      vector<VariableLocation> &lvlocs=j->getLocationLists();
      for(auto k: lvlocs) {
	// this unfortunately seems to be happening. It may either be a
	// problem with the DWARF or a problem with dyninst.
	if( k.lowPC == 0 || k.hiPC ==0xFFFFFFFFFFFFFFFF){
	  *errfile << "DWARF Warning: Location List for " << j->getName()
		   << " from " << i->getName() << " seems insane [" << hex
		   << k.lowPC << ',' << k.hiPC << dec << "]: skipping\n";
	  continue;
	}
	if( k.lowPC < i->getOffset() || k.hiPC > i->getOffset()+i->getSize()){
	  *errfile << "DWARF Warning: Location "
	       << (k.lowPC < i->getOffset() ? k.lowPC : k.hiPC) << " for "
	       << j->getName() << " from " << i->getName()
	       << " is out of range for the function [" << i->getOffset()
	       << ',' << i->getOffset()+i->getSize() << "].\n";
	}
	discrete_interval<Address> addr_inter
	  = construct<discrete_interval<Address> >
	  (k.lowPC,k.hiPC,interval_bounds::closed());

	// if(verbose){
	//   cout << "\t\t[" << hex << k.lowPC << dec;
	//   vector<Statement *> lines;
	//   if( i->getModule()->getSourceLines(lines, k.lowPC))
	//     for(auto l:lines)
	//       cout << ' ' << l->getFile() << ':' << l->getLine() << 'c'
	// 	   << l->getColumn();
	//   lines.clear();
	//   cout << ',' << hex << k.hiPC << dec;
	//   if( i->getModule()->getSourceLines( lines, k.hiPC))
	//     for(auto l:lines)
	//       cout << ' ' << l->getFile() << ':' << l->getLine() << 'c'
	// 	   << l->getColumn();
	//   cout << ']' << endl;
	// }

	LVarSet newone;
	pair<localVar*,Function*> newpair;
	newpair.first=j;
	newpair.second=i;
	newone.insert(newpair);
	llmap.add(make_pair(addr_inter,newone));
      }
    }
  }

  // read in all the files
  std::map< string, vector<line_data > > file_lines;
  for(auto f: files)
    read_file(file_lines,f.file_name);

  // insert the variable declarations 
  for( auto i: funcs) {
    vector <localVar *> lvars;
    i->getParams(lvars);
    i->getLocalVariables(lvars);
    for(auto j: lvars){
      auto line=j->getLineNum();
      if(file_lines.find(j->getFileName()) == file_lines.end()){
	if(j->getFileName().empty()){
	  *errfile << "DWARF Warning: variable " << i->getName() << ':'
		   << j->getName() << " declared in a file with no name.\n";
	  continue;
	}
	if(find(files.begin(),files.end(), j->getFileName()) == files.end()){
	  *errfile << "DWARF Warning: variable " << i->getName() << ':'
		   << j->getName() << " declared in an unknown file "
		   << j->getFileName() << ".\n";
	  continue;
	} 
	*errfile << "Warning: variable " << i->getName() << ':' << j->getName()
		 << " declared in a file " << j->getFileName()
		 << " that could not be read.\n";
	continue;
      }
      if(i->getModule()->fullName() != j->getFileName()){
	string basename=j->getFileName().substr( j->getFileName().rfind('/')+1);
	if( basename == i->getModule()->fullName()){
	  *errfile << "Dyninst bug: module.fullName() missing path for "
		   << j->getFileName() << ".\n";
	}else {
	  *errfile << "DWARF Warning: " << j->getName() << " is from "
		   << i->getName() << " in CU " << i->getModule()->fullName()
		   << " but was declared in " << j->getFileName() << ".\n";
	}
      }
      if(line==0){
	*errfile << "DWARF Warning: variable " << i->getName() << ':'
		 << j->getName() << " declared on line 0. skipping.\n";
	continue;
      }
      if(line<0){
	*errfile << "DWARF Warning: variable " << i->getName() << ':'
		 << j->getName() << " declared on negative line number " << line
		 << ". skipping.\n";
	continue;
      }
      if(line>=file_lines[ j->getFileName()].size()){
	*errfile << "DWARF Warning: variable " << i->getName() << ':'
		 << j->getName() << " declared line number " << line
		 << " but file " << j->getFileName() <<  " only has "
		 << file_lines[ j->getFileName()].size()
		 << " lines. skipping.\n";
	continue;
      }

      // the linenum-1 because array indexes begin with 0 and line numbers
      // start at 1
      line--;
      file_lines[ j->getFileName()][line].decls.insert(j);
    }
  }
  
  for(auto i: llmap) { // iterate through all the intervals
    // in llmap first is the address_interval and
    // second is a set of pairs. pair<localVar*,Function*>

    // iterate through all the variables in that interval
    for( auto j: i.second){
      auto funcp=j.second;
      for(auto pc = i.first.lower(); pc<=i.first.upper(); pc++){
	std::vector<Statement *> lines;
	if( !funcp->getModule()->getSourceLines(lines, pc)){
	  *errfile << "DWARF Warning: No line info for " << hex
		   << i.first.lower() << dec << endl;
	  continue;
	}
	for( auto l: lines){
	  // if we haven't read this file yet
	  // we assume that it must be inlined because it is not a source file
	  // for the CU where the function was defined.
	  if(file_lines.find(l->getFile()) == file_lines.end()){
	    if(verbose)
	      *errfile << "Info: pulling in unreferenced file " << l->getFile()
		       << endl;
	    if(read_file( file_lines, l->getFile()))
	      files.insert( file_data(l->getFile(),true));
	    else
	      // if it is some source file that we can't read we can't do
	      // anything anyway.
	      continue;
	  }
	  auto line=l->getLine();
	  if(line>= 1 && line <= file_lines[l->getFile()].size())
	    file_lines[l->getFile()][line-1].avail.insert(j.first);
	  else
	    warn_line_range( l->getFile(), funcp->getName(), j.first->getName(),
			     line, file_lines[l->getFile()].size());
	} // the statements for that pc
      } // the pc's 
    } // all the varibles within that interval
  } // each interval

  if(!quiet)
    for( auto f: files){
      if(f.inlined && !verbose)
	continue;
      if(!machine)
	cout << "***** " << f.file_name << "------" << endl;
      unsigned lineno=0;
      for( auto l: file_lines[f.file_name]) {
	lineno++;
	if(!machine){
	  cout << lineno << ' ' << l.line << endl;
	  if( l.decls.size()+l.avail.size() != 0){
	    cout << "\t// ";
	    if(l.decls.size() != 0){
	      cout << " Decl: ";
	      for( auto v: l.decls)
		cout << v->getName() << ' ';
	    }
	    if(l.avail.size() != 0){
	      cout << " Avail: ";
	      for( auto v: l.avail)
		cout << v->getName() << ' ';
	    }
	  }
	  // cout << endl;
	} else { //machine readable
	  if( l.decls.size()+l.avail.size()==0)
	    continue;
	  cout << f.file_name << ':' << lineno << ' ';
	  if(l.decls.size() != 0){
	    cout << " D: ";
	    for( auto v: l.decls)
	      cout << v->getName() << ' ';
	  }
	  if(l.avail.size() != 0){
	    cout << " A: ";
	    for( auto v: l.avail)
	      cout << v->getName() << ' ';
	  }
	  cout << endl;
	} // machine readable or not
      } // more lines in the file
    } // more files
  // not quiet
}
