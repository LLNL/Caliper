// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// Modified by Aimee Sylvia
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/// @file Expand.cpp
/// Print expanded records

#include "Expand.h"

#include "CaliperMetadataDB.h"

#include "Attribute.h"
#include "ContextRecord.h"
#include "Node.h"

#include "util/split.hpp"

#include <algorithm>
#include <functional>
#include <iterator>
#include <set>


using namespace cali;
using namespace std;

struct Expand::ExpandImpl
{
    set<string> m_selected;
    set<string> m_deselected;

  struct Field {
    string attr_name;
    Attribute attr;

    int width;
    char align; // 'l', 'r', 'c'
  }; // for the frmtparse function

    vector<Field> m_fields; // for the format strings

  string title;

    ostream&    m_os;

    ExpandImpl(ostream& os)
        : m_os(os)
        { }

    void parse(const string& field_string) {
        vector<string> fields;

        util::split(field_string, ':', back_inserter(fields));

        for (const string& s : fields) {
            if (s.size() == 0)
                continue;

            if (s[0] == '-')
                m_deselected.emplace(s, 1, string::npos);
            else
                m_selected.insert(s);
        }
    }

  // based on SnapshotTextFormatter::parse
  void frmtparse(const string& formatstring, const string& titlestring) {
    // parse format: "%[<width+alignment(l|r|c)>]attr_name%..."
    if (!titlestring.empty())
      m_os << titlestring;
    vector<string> split_string;

    util::split(formatstring, '%', back_inserter(split_string));

    while (!split_string.empty()) {
      Field field = { "", Attribute::invalid, 0, 'l' };

      // nixed the prefixes

      // parse field entry

      vector<string> field_strings;
      util::tokenize(split_string.front(), "[]", back_inserter(field_strings));

      // look for width/alignment specification (in [] brackets)

      int wfbegin = -1;
      int wfend = -1;
      int apos = -1;

      int nfields = field_strings.size();

      for (int i = 0; i < nfields; ++i)
	if(field_strings[i] == "[")
	  wfbegin = i;
	else if (field_strings[i] == "]")
	  wfend = i;

      if (wfbegin >= 0 && wfend > wfbegin+1) {
	// width field specified
	field.width = stoi(field_strings[wfbegin+1]);

	if (wfbegin > 0)
	  apos = 0;
	else if (wfend+1 < nfields)
	  apos = wfend+1;
      } else if (nfields > 0)
	apos = 0;

      if (apos >= 0)
	field.attr_name = field_strings[apos];

      split_string.erase(split_string.begin());

      m_fields.push_back(field);
    }
  }

    void print(CaliperMetadataDB& db, const RecordMap& rec) {
        int nentry = 0;

        for (auto const &entry : ContextRecord::unpack(rec, std::bind(&CaliperMetadataDB::node, &db, std::placeholders::_1))) {
            if (entry.second.empty())
                continue;
            if ((!m_selected.empty() && m_selected.count(entry.first) == 0) || m_deselected.count(entry.first))
                continue;

            m_os << (nentry++ ? "," : "") << entry.first << "=";

            int nelem = 0;
            for (auto it = entry.second.rbegin(); it != entry.second.rend(); ++it)
                m_os << (nelem++ ? "/" : "") << (*it).to_string();
        }

        if (nentry > 0)
            m_os << endl;
    }

  void print(CaliperMetadataDB& db, const EntryList& list) {
    int nentry = 0;

    // differentiate which kind of printing to do pending on format strings
    if (m_fields.empty()) {
      // this is exactly the same as the regular Expand.cpp
      for (const Entry& e : list) {
	if (e.node()) {
	  vector<const Node*> nodes;

	  for (const Node* node = e.node(); node && node->attribute() != CALI_INV_ID; node = node->parent()) {
	    string name = db.attribute(node->attribute()).name();

	    if ((!m_selected.empty() && m_selected.count(name) == 0) || m_deselected.count(name))
	      continue;

	    nodes.push_back(node);
	  }

	  if (nodes.empty())
	    continue;

	  stable_sort(nodes.begin(), nodes.end(), [](const Node* a, const Node* b) { return a->attribute() < b->attribute(); } );
	  
	  cali_id_t prev_attr_id = CALI_INV_ID;

	  for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
	    if ((*it)->attribute() != prev_attr_id) {
	      m_os << (nentry++ ? "," : "") << db.attribute((*it)->attribute()).name() << '=';
	      prev_attr_id = (*it)->attribute();
	    } else {
	      m_os << '/';
	    }
	    m_os << (*it)->data().to_string();
	  }
	} else if (e.attribute() != CALI_INV_ID) {
	  string name = db.attribute(e.attribute()).name();

	  if ((!m_selected.empty() && m_selected.count(name) == 0) || m_deselected.count(name))
	    continue;

	  m_os << (nentry++ ? "," : "") << name << '=' << e.value();
	}
      }
      if (nentry > 0)
	m_os << endl;
    } else {
      string printstr;
      // this is for the formatted table printing
      for (Field f : m_fields) {
	int outputcheck = 0; // this is dumb but I don't know a better way to do this
	for (const Entry& e: list) {
	  // pretty much the same as non-formatted section
	  if (e.node()) {
	    vector<const Node*> nodes;

	    string name;
	    for (const Node* node = e.node(); node && node->attribute() != CALI_INV_ID; node = node->parent()) {
	      name = db.attribute(node->attribute()).name();

	      nodes.push_back(node);
	    }

	    if (nodes.empty())
	      continue;

	    stable_sort(nodes.begin(), nodes.end(), [](const Node* a, const Node* b) { return a->attribute() < b->attribute(); } );

	    cali_id_t prev_attr_id = CALI_INV_ID;

	    string valuestr = "";
	    string whitespace = "                              ";
	    string space;
	    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
	      if (db.attribute((*it)->attribute()).name().compare(f.attr_name) == 0) {
		if ((*it)->attribute() != prev_attr_id) {
		  prev_attr_id = (*it)->attribute();
		} else {
		  valuestr.append("/");
		}
		
		valuestr.append((*it)->data().to_string());
	      }
	    }
	    if (!valuestr.empty()) {
	      // print the value and the buffer space
	      space = (valuestr.length() < f.width ? (whitespace.substr(0,(f.width - valuestr.length()))) : "");
	      printstr << valuestr << space;
	      outputcheck = 1;
	    }
	  } else if (e.attribute() != CALI_INV_ID) {
	    string name = db.attribute(e.attribute()).name();

	    string whitespace = "                              ";
	    string space;
	    if (f.attr_name.compare(name) == 0) {
	      string valuestr = e.value().to_string();
	      // print the value and the buffer space
	      space = (valuestr.length() < f.width ? (whitespace.substr(0,(f.width - valuestr.length()))) : "");
	      printstr << valuestr << space;
	      outputcheck = 1;
	    }
	  }
	}
	// check for a value added, if no value added (no match in the entryline for
	// the m_field field) then print blank of f.width
	if (outputcheck == 0) {
	  string whitespace = "                              ";
	  string space = whitespace.substr(0,f.width);
	  printstr << space;
	}
      }
      // this checks for an empty line. if empty, skips printing it
      if (printstr.find_first_not_of(" ") >= 0){
	m_os << printstr << endl;
      }
    }
  }
};


Expand::Expand(ostream& os, const string& field_string, const string& formatstr, const string& titlestr)
    : mP { new ExpandImpl(os) }
{
    mP->parse(field_string);
    // if format is defined, create the format fields vector
    if (!formatstr.empty())
      mP->frmtparse(formatstr,titlestr);
}

Expand::~Expand()
{
    mP.reset();
}

void
Expand::operator()(CaliperMetadataDB& db, const RecordMap& rec) const
{
    mP->print(db, rec);
}

void 
Expand::operator()(CaliperMetadataDB& db, const EntryList& list) const
{
    mP->print(db, list);
}
