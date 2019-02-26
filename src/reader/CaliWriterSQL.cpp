// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
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

/// \file CsvWriter.cpp
/// CsvWriter implementation

#include "caliper/reader/CaliWriterSQL.h"
#include "sqlite/sqlite3.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/Node.h"
#include "caliper/common/OutputStream.h"

#include "../common/util/write_util.h"

#include <mutex>
#include <set>

using namespace cali;

namespace
{

const char* esc_chars { "\\,=\n" }; ///< characters that need to be escaped


} // namespace [anonymous]

const char* global_query =        "INSERT INTO globals        (run_id, attr, global_name, global_value) VALUES (?,?,?,?);";
const char* node_query =          "INSERT INTO nodes          (run_id, node_id, attr, data, parent) VALUES (?,?,?,?,?);";
const char* measurement_query =   "INSERT INTO measurements   (run_id, node_id, attr, data) VALUES (?,?,?,?);";

struct CaliWriterSQL::CaliWriterSQLImpl
{
    std::string  m_os;
    std::mutex    m_os_lock;

    std::set<cali_id_t> m_written_nodes;
    std::mutex    m_written_nodes_lock;

    std::size_t   m_num_written;
    sqlite3* db_connection;
    sqlite3_stmt* global_stmt;
    sqlite3_stmt* measurement_stmt;
    sqlite3_stmt* node_stmt;
    bool file_exists(const std::string& name){
      return false; // TODO: real
    }
    sqlite3_stmt* prepare_query(const char* query){
        sqlite3_stmt* out;
        const char* unused;
        sqlite3_prepare(db_connection,
             query,
             std::string(query).size() * sizeof(char),
             &out,
             &unused
        );
        return out;

    }
    void insert_node(int runId, int nodeId, int attrId, std::string data, int parentId){
       sqlite3_bind_int(node_stmt,1,runId);
       sqlite3_bind_int(node_stmt,2,nodeId);
       sqlite3_bind_int(node_stmt,3,attrId);
       sqlite3_bind_text(node_stmt,4,data.c_str(),data.size()*sizeof(char),[](void*){});
       sqlite3_bind_int(node_stmt,5,parentId);
       int errcode = sqlite3_step(node_stmt);
       sqlite3_reset(node_stmt);
    }
    void insert_measurement(int nodeId, int attrId, std::string data){
       static int hack;
       sqlite3_bind_int(measurement_stmt,1,++hack);
       sqlite3_bind_int(measurement_stmt,2,nodeId);
       sqlite3_bind_int(measurement_stmt,3,attrId);
       sqlite3_bind_text(measurement_stmt,4,data.c_str(),data.size()*sizeof(char),[](void*){});
       int errcode = sqlite3_step(measurement_stmt);
       sqlite3_reset(measurement_stmt);
    }
    void insert_global(int nodeId, int attrId, std::string global_name,  std::string data){
       static int hack;
       sqlite3_bind_int(global_stmt,1,++hack);
       sqlite3_bind_int(global_stmt,2,attrId);
       sqlite3_bind_text(global_stmt,3,global_name.c_str(),global_name.size()*sizeof(char),[](void*){});
       sqlite3_bind_text(global_stmt,4,data.c_str(),data.size()*sizeof(char),[](void*){});
       int errcode = sqlite3_step(global_stmt);
       sqlite3_reset(global_stmt);
    }
    void write_node_content(const CaliperMetadataAccessInterface& db, const cali::Node* node)
    {
        static int hack;
        //os << "__rec=node,id=" << node->id()
        //   << ",attr="         << node->attribute();
        int id = node->id(); 
        int attr = node->attribute(); 
        std::string data = node->data().to_string();
        //util::write_esc_string(os << ",data=", node->data().to_string(), ::esc_chars);
        int parentId = -1;
        if (node->parent() && node->parent()->id() != CALI_INV_ID){
            parentId = node->parent()->id();
            //os << ",parent=" << node->parent()->id();
        }
        insert_node(++hack, id, attr, data, parentId); 
        auto AttrObject = db.get_attribute(attr);
        if(AttrObject.properties() & CALI_ATTR_GLOBAL){
            std::string name = AttrObject.name();
            insert_global(id, attr, name, data);
        }
        //os << '\n';
    }
    const Node* chase_nested_attribute(const CaliperMetadataAccessInterface& db, const Entry& in){
        const Node* node;
        for(node = in.node(); node; node=node->parent()){
            if(db.get_attribute(node->attribute()).properties() & CALI_ATTR_NESTED){
              break;
            }
        }
        return node;
    }
    void write_record_content(const CaliperMetadataAccessInterface& db, const char* record_type, int nr, int ni, const std::vector<Entry>& rec) {
        //os << "__rec=" << record_type;
                
        // write reference entries
        int nested_id = -1;
        if (nr > 0) {
            //os << ",ref";
            int ref = 0;
            for (const Entry& e : rec) {
                if (e.is_reference()) {
                    const Node * tree_ref = chase_nested_attribute(db,e);
                    if(tree_ref) {
                        nested_id = tree_ref->id();
                    }
                }
            }
        }
    
        // write immediate entries
        std::vector<int> attributes;
        std::vector<std::string> values;
        if (ni > 0) {
            //os << ",attr";
    
            for (const Entry& e : rec) {
                if (e.is_immediate()) {
                    attributes.push_back(e.attribute());
                    //os << '=' << e.attribute();
                }
            }
            //os << ",data";
    
            for (const Entry& e : rec) {
                if (e.is_immediate()) {
                    //util::write_esc_string(os << '=', e.value().to_string(), esc_chars);
                    values.push_back(e.value().to_string());
                }
            }
        }
        
        for( int index = 0; index < attributes.size() ; ++index ) {
            insert_measurement(nested_id, attributes[index], values[index]);
        }
    
    }

    bool database_exists(){
      return db_connection!=nullptr;
    }

    void prepare_tables(){
       sqlite3_exec(
              db_connection,
              "CREATE TABLE globals ( run_id int PRIMARY KEY, attr int, global_name varchar(255), global_value varchar(255) );",
              [](void*, int, char**, char**){return 0;},
              nullptr,
              nullptr
          );
       sqlite3_exec(
              db_connection,
             "CREATE TABLE nodes (run_id int PRIMARY KEY, node_id int , attr int, data varchar(255), parent int);",
              [](void*, int, char**, char**){return 0;},
              nullptr,
              nullptr
          );
       sqlite3_exec(
              db_connection,
             "CREATE TABLE measurements (run_id int PRIMARY KEY, node_id int , attr int, data varchar(255));",
              [](void*, int, char**, char**){return 0;},
              nullptr,
              nullptr
          );
    }
    void open_transaction(){
      char *msg;
      sqlite3_exec(db_connection, "BEGIN TRANSACTION",nullptr,nullptr,&msg);
    }
    void close_transaction(){
      char* msg;
      sqlite3_exec(db_connection,"COMMIT TRANSACTION",nullptr, nullptr, &msg);
      sqlite3_exec(
          db_connection,
          R"queryString(
            CREATE VIEW calltree AS
            WITH ancestors AS
            (
             SELECT node_id, parent, data AS pathname FROM nodes
             WHERE (nodes.parent = -1) OR (nodes.data LIKE 'main' AND nodes.attr IN roots)
             union all
             SELECT a.node_id,a.parent, ancestors.pathname || '/' ||  a.data AS pathname
             FROM nodes a
             INNER JOIN ancestors ON ancestors.node_id=a.parent WHERE a.data NOT LIKE 'main'
            )
            SELECT * FROM ancestors WHERE pathname LIKE 'main%';
          )queryString",
          [](void*, int, char**, char**){return 0;},
          nullptr,
          nullptr
      );
      sqlite3_exec(
            db_connection,
            "CREATE VIEW roots AS select node_id FROM nodes WHERE data LIKE 'function';",
            [](void*, int, char**, char**){return 0;},
            nullptr,
            nullptr
      );
    }
    CaliWriterSQLImpl(const std::string& os)
        : m_os(os),
          m_num_written(0)
    { 

        if(file_exists(os)){
            sqlite3_open_v2(
                os.c_str(), 
                &db_connection, 
                SQLITE_OPEN_READWRITE,
                nullptr
            ); 
        }
        else{
            sqlite3_open_v2(
                os.c_str(), 
                &db_connection, 
                SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 
                nullptr
            ); 
            if(database_exists()){
              prepare_tables();
            }
        }

        if(!database_exists()){
          std::cerr << "Banzai: dead database\n";
        }
        else{
            global_stmt = prepare_query(global_query);
            node_stmt = prepare_query(node_query);
            measurement_stmt = prepare_query(measurement_query);
        }
        open_transaction();
    }
    ~CaliWriterSQLImpl(){
      close_transaction();
    }
    
    void recursive_write_node(const CaliperMetadataAccessInterface& db, cali_id_t id)
    {
        if (id < 11) // don't write the hard-coded metadata nodes
            return;

        {
            std::lock_guard<std::mutex>
                g(m_written_nodes_lock);

            if (m_written_nodes.count(id) > 0)
                return;
        }

        Node* node = db.node(id);

        if (!node)
            return;

        recursive_write_node(db, node->attribute());

        Node* parent = node->parent();

        if (parent && parent->id() != CALI_INV_ID)
            recursive_write_node(db, parent->id());

        {
            std::lock_guard<std::mutex>
                g(m_os_lock);
            
            write_node_content(db,node);
            ++m_num_written;
        }

        {
            std::lock_guard<std::mutex>
                g(m_written_nodes_lock);

            if (m_written_nodes.count(id) > 0)
                return;
       
            m_written_nodes.insert(id);
        }
    }
    void write_globals(const CaliperMetadataAccessInterface& db,
                         const std::vector<Entry>& rec)
    {
        // write node entries; count the number of ref and immediate entries
        
        int nr = 0;
        int ni = 0;
        
        for (const Entry& e : rec) {
            if (e.is_reference()) {
                recursive_write_node(db, e.node()->id());
                ++nr;
            } else if (e.is_immediate()) {
                recursive_write_node(db, e.attribute());
                ++ni;
            }
        }

    }

    void write_entrylist(const CaliperMetadataAccessInterface& db,
                         const char* record_type,
                         const std::vector<Entry>& rec)
    {
        // write node entries; count the number of ref and immediate entries
         
        int nr = 0;
        int ni = 0;
        char* errorMessage;
        for (const Entry& e : rec) {
            if (e.is_reference()) {
                recursive_write_node(db, e.node()->id());
                ++nr;
            } else if (e.is_immediate()) {
                recursive_write_node(db, e.attribute());
                ++ni;
            }
        }

        // write the record
        
        {
            std::lock_guard<std::mutex>
                g(m_os_lock);

            write_record_content(db,record_type, nr, ni, rec);
            ++m_num_written;
        }
    }
};


CaliWriterSQL::CaliWriterSQL(const std::string& os)
    : mP(new CaliWriterSQLImpl(os))
{ }

CaliWriterSQL::~CaliWriterSQL()
{
    mP.reset();
}

size_t CaliWriterSQL::num_written() const
{
    return mP ? mP->m_num_written : 0;
}

void CaliWriterSQL::write_snapshot(const CaliperMetadataAccessInterface& db, const std::vector<Entry>& list)
{
    mP->write_entrylist(db, "ctx", list);
}

void CaliWriterSQL::write_globals(const CaliperMetadataAccessInterface& db, const std::vector<Entry>& list)
{
    mP->write_globals(db, list);
}
