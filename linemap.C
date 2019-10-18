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
  typedef set< pair<localVar*,Function*> > LVarSet;
  interval_map<Address, LVarSet> llmap;
  //iterate through all the functions
  vector <Function *> funcs;
  if (!obj->getAllFunctions(funcs))
    exit(EXIT_NOFUNCS);

  set< string> files;
  for( auto i: funcs) {
    files.insert( i->getModule()->fullName());
    if(verbose)
      cout << endl << "Func: " << i->getName() << endl;
    //iterate through all the local variables and parameters
    vector <localVar *> lvars;
    i->getParams(lvars);
    i->getLocalVariables(lvars);
    for(auto j: lvars) {
      files.insert( j->getFileName());
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
	  cerr << "Location List for " << j->getName() << " from "
	       << i->getName() << " seems insane [" << hex << k.lowPC << ','
	       << k.hiPC << "]: skipping\n";
	  continue;
	}
	discrete_interval<Address> addr_inter
	  = construct<discrete_interval<Address> >
	  (k.lowPC,k.hiPC,interval_bounds::closed());
	if(verbose){
	  cout << "\t\t[" << hex << k.lowPC << dec;
	  vector<Statement *> lines;
	  if( i->getModule()->getSourceLines(lines, k.lowPC))
	    for(auto l:lines)
	      cout << ' ' << l->getFile() << ':' << l->getLine() << 'c'
		   << l->getColumn();
	  lines.clear();
	  cout << ',' << hex << k.hiPC << dec;
	  if( i->getModule()->getSourceLines( lines, k.hiPC))
	    for(auto l:lines)
	      cout << ' ' << l->getFile() << ':' << l->getLine() << 'c'
		   << l->getColumn();
	  cout << ']' << endl;
	}
	LVarSet newone;
	pair<localVar*,Function*> newpair;
	newpair.first=j;
	newpair.second=i;
	newone.insert(newpair);
	llmap.add(make_pair(addr_inter,newone));
      }
    }
  }

  struct line_data{
    string line;
    vector< localVar*> decls;
    vector< localVar*> avail;
    line_data(string &l):line(l){}
    line_data(){};
  };
  
  // read in all the files
  std::map< string, vector<line_data > > file_lines;
  for(auto f: files){
    ifstream inf(f);
    file_lines.insert(pair<string,vector<line_data> >(f,vector<line_data>()));
    string line;
    while(getline(inf,line))
      file_lines[f].push_back(line_data(line));
    inf.close();
  }

  // insert the variable declarations 
  for( auto i: funcs) {
    vector <localVar *> lvars;
    i->getParams(lvars);
    i->getLocalVariables(lvars);
    for(auto j: lvars){
      auto line=j->getLineNum();
      if(line==0){
	cerr << "Warning: variable " << i->getName() << ':' << j->getName()
	     << " declared on line 0. skipping.\n";
	continue;
      }
      if(line<0){
	cerr << "Warning: variable " << i->getName() << ':' << j->getName()
	     << " declared on negative line number " << line << ". skipping.\n";
	continue;
      }
      if(line>=file_lines[ j->getFileName()].size()){
	cerr << "Warning: variable " << i->getName() << ':' << j->getName()
	     << " declared line number " << line << " but file only has "
	     << file_lines[ j->getFileName()].size() << "lines. skipping.\n";
	continue;
      }
      // the linenum-1 because array indexes begin with 0 and line numbers
      // start at 1
      line--;
      file_lines[ j->getFileName()][line].decls.push_back(j);
    }
  }
  
  for(auto i: llmap) { // iterate through all the intervals
    // in llmap first is the address_interval and
    // second is a set of pairs. pair<localVar*,Function*>

    // iterate through all the variables in that interval
    for( auto j: i.second){
      auto funcp=j.second;
      vector<Statement *> low_lines,high_lines;
      if( !funcp->getModule()->getSourceLines(low_lines, i.first.lower())){
	cerr << "No line info for " << i.first.lower() << dec << endl;
	continue;
      }
      if( !funcp->getModule()->getSourceLines(high_lines, i.first.upper())){
	cerr << "No line info for " << i.first.upper() << dec << endl;
	continue;
      }

      // I have no idea what to do if they aren't the same length
      // I believe they are supposed to be parallel vectors
      assert(low_lines.size() == high_lines.size());

      for( auto idx=0;idx<low_lines.size();idx++){
	auto &file=low_lines[idx]->getFile();
	auto low_line=low_lines[idx]->getLine();
	auto high_line=high_lines[idx]->getLine();

	// lines are not in the same file
	if(file.compare(high_lines[idx]->getFile()) != 0){
	  // just mark the high line and low line. It is not a range.
	  if(low_line >= 1 && low_line < file_lines[file].size())
	    file_lines[file][low_line].avail.push_back(j.first);
	  else
	    cerr << "Warning: " << funcp->getName() << ':' << j.first->getName()
		 << " line number out of range: " << low_line << '/'
		 << file_lines[file].size() << endl;
	  if(high_line >= 1 && high_line < file_lines[file].size())
	    file_lines[file][high_line].avail.push_back(j.first);
	  else
	    cerr << "Warning: " << funcp->getName() << ':' << j.first->getName()
		 << " line number out of range: " << high_line << '/'
		 << file_lines[file].size() << endl;
	}else{
	  // mark the range
	  for( auto cur_line=low_lines[idx]->getLine();
	       cur_line<high_lines[idx]->getLine();
	       cur_line++){
	    if(cur_line >= 1 && cur_line < file_lines[file].size())
	      file_lines[file][cur_line].avail.push_back(j.first);
	    else
	      cerr << "Warning: " << funcp->getName() << ':' << j.first->getName()
		   << " line number out of range: " << cur_line << '/'
		   << file_lines[file].size() << endl;
	  }
	}
      } // all the lines for that interval
    } // all the varibles within that interval
  } // each interval

  for( auto f: files){
    cout << "***** " << f << "------" << endl;
    for( auto l: file_lines[f]) {
      cout << l.line;
      if( l.decls.size()+l.avail.size() != 0){
	cout << "// ";      
	if(l.decls.size() != 0){
	  cout << "Decl: ";
	  for( auto v: l.decls)
	    cout << v->getName() << ' ';
	}
	if(l.avail.size() != 0){
	  cout << "Avail: ";
	  for( auto v: l.avail)
	    cout << v->getName() << ' ';
	}
	cout << endl;
      }
    }
  }
}
