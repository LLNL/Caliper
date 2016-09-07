// This file written by Aimee Sylvia.

#include <Args.h>

#include <util/split.hpp>

#include "../../services/textlog/TextLog.cpp"

#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <map>
#include <algorithm>
#include <iostream>
#include <iterator>


using namespace std;
using namespace util;

/*
Borrows create_default_formatstring from TextLog.cpp
Borrows from parse() in SnapshotTextFormatter.cpp
 */

// define the namespace

namespace 

{
  const char* usage = "cali-print [OPTIONS]... [FILE]..."
     "\n Print a report from a cali-query output file.";

  const Args::Table option_table[] = {
    // name, longopt name, shortopt char, has argument, info, argument info
    { "format", "format", 'f', true,  "Set the format of the data",      "FORMAT_STRING" },
    { "select", "select", 's', true,  "Select the attributes to print",  "QUERY_STRING"  },
    { "title",  "title",  't', true,  "Set the title and/or header row", "STRING"        },
    { "output", "output", 'o', true,  "Set the output file name",        "FILE"          },
    { "help",   "help",   'h', false, "Print help message",              nullptr         },
    Args::Table::Terminator
  };
}

string parse_print(const string formatstr, const string queryline);

void QueryFormatter(ostream& os, string format, string title, string file) {
  // initialize the input file
  ifstream query (file, ifstream::in);
  char line[512];
  string formatline;
  // print the header
  os << title << endl;
  // get and format the input from cali-query
  while (query.getline(line,512)) {
    formatline = parse_print(format, line);
    os << formatline << endl;
  };
}

// where formatstr is in "%[w1]attr1% %[w2]attr2%" format and queryline is in "attr1=val1,attr2=val2" format
string parse_print(const string formatstr, const string queryline) { // heavily borrowed from SnapshotTextFormatter.cpp
  vector<string> split_string;

  split(formatstr,'%',back_inserter(split_string));

  vector<string> frmtname;
  vector<int> frmtwidth;
  string str = "";

  while(!split_string.empty()) {
    vector<string> field_strings;
    tokenize(split_string.front(),"[]", back_inserter(field_strings));

    int wfbegin = -1;
    int wfend = -1;
    int apos = -1;
    
    int nfields = field_strings.size();
    
    for (int i = 0; i < nfields; ++i) {
      if (field_strings[i].compare("[") == 0)
	wfbegin = i;
      else if (field_strings[i].compare("]") == 0)
	wfend = i;
    }

    if (wfbegin >=0 && wfend > wfbegin+1) {
      frmtwidth.push_back(stoi(field_strings[wfbegin+1]));
      
      if (wfbegin >0)
	apos = 0;
      else if(wfend+1 < nfields) {
	apos = wfend+1;
      }
    }
    else if (nfields >0) {
      if (wfbegin > 0)
	apos = 0;
      else 
	apos = wfend+1;
      if (nfields == 2) {
	apos = -1;
	frmtname.push_back("");
      }
    }
    
    
    if (apos >= 0) {
      frmtname.push_back(field_strings[apos]);
      if (frmtwidth.size() < frmtname.size())
	frmtwidth.push_back(frmtname.back().length()+2);
    }
    split_string.erase(split_string.begin());
  }
  // now compare and strip and print
  vector<string> split_query;
  
  split(queryline,',',back_inserter(split_query));
  
  map<string,string> query_map;

  while(!split_query.empty()) {
    vector<string> holding_string;
    tokenize(split_query.front(), "=", back_inserter(holding_string));
    query_map[holding_string.front()] = holding_string.back();
    split_query.erase(split_query.begin());
  }
  // then match the print value to the width via map search and print
  for (int i = 0; i < frmtname.size(); ++i) {
    if (frmtname[i].compare("") != 0) {
      int len = query_map[frmtname[i]].length();
      string whitespace = "                                        ";
      int w = ( (len < frmtwidth[i]) ? (frmtwidth[i] - len) : 0 );
      string space = whitespace.substr(0,w);
      str += query_map[frmtname[i]];
      str += space;
    }
  }   
  return str;
}


string create_default_formatstring(const vector<string>& attr_names) {
  if (attr_names.size() < 1)
    return "%time.inclusive.duration%";

  int name_sizes = 0;

  for (const string& s : attr_names)
    name_sizes = max<int>(name_sizes, s.size());

  ostringstream os;

  for (const string& s : attr_names)
    os << "%[" << name_sizes << "]" << s << "% ";

  os << endl;

  return os.str();
}
 

int main(int argc, const char* argv[])
{
  Args args(::option_table);

  // parse arguments

  int i = args.parse(argc, argv);

  if (i < argc) {
    cerr << "cali-print: error: unknown option: " << argv[i] << '\n'
	 << "  Available options: ";

    args.print_available_options(cerr);

    return -1;
  }

  if (args.is_set("help")) {
    cerr << usage << "\n\n";
    
    args.print_available_options(cerr);
    
    return 0;
  }


  // check input files

  if (args.arguments().empty()) {
    cerr << "cali-print: Input file required" << endl;
    return -2;
  }
  if (args.arguments().size() > 1) {
    cerr << "cali-print: Only one input file is accepted" << endl;
    return -2;
  }

  // create output stream if designated

  ofstream fs;

  if (args.is_set("output")) {
    string filename = args.get("output");

    fs.open(filename.c_str());

    if (!fs) {
      cerr << "cali-print: error: could not open output file "
	   << filename << endl;

      return -2;
    }
  }

  vector<string> attr_names;

  // for select, we could just util/split the attr1:attr2:... list into a vector<string> and pop it into autoformatter
  // brilliant!
  if (args.is_set("select")) {
    if (!args.get("select").empty()) {
      split(args.get("select"),':',back_inserter(attr_names));
    }
    else {
      cerr << "cali-print: error: Arguments required for --select" << endl; 
      return -2;
    }
  }
  else {
    map<string,string> attrs;
    ifstream queryfile (args.arguments().front(), ifstream::in);
    char line[512];
    while (queryfile.getline(line,512)) {
      vector<string> entries;
      string linestr(line);
      split(linestr,',',back_inserter(entries));
      for (string entry : entries) {
	if ((entry.compare(0,6,"event.") != 0) && (entry.compare(0,5,"cali.") != 0)) {
	  vector<string> hold;
	  split(entry,'=',back_inserter(hold));
	  if( attrs.find(hold.front()) == attrs.end()) {
	    attrs.insert(pair<string,string>(hold.front(),hold.front()));
	  }
	}
      }
    };
    for (map<string,string>::iterator it=attrs.begin(); it != attrs.end(); ++it) {
      attr_names.push_back(it->second);
    }
  }

// create the query output formatting
  string formatstr = args.get("format");
  if (formatstr.size() == 0)
    formatstr = create_default_formatstring(attr_names);

// print the header string (??), then print the (filtered?) formatted query outputs
  string titlestr = args.get("title"); 
  if (titlestr.size() == 0){
    string titlelist = "";
    vector<string> names_copy = attr_names;
    while (!names_copy.empty()) {
      titlelist += names_copy.front();
      titlelist += "=";
      titlelist += names_copy.front();
      titlelist += ",";
      names_copy.erase(names_copy.begin());
    }
    titlestr = parse_print(formatstr, titlelist);
  }
  QueryFormatter(fs.is_open() ? fs : cout, formatstr, titlestr, args.arguments().front());
}

