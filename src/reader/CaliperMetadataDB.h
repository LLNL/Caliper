/// @file CaliperMetadataDB
/// CaliperMetadataDB class declaration

#ifndef CALI_CALIPERMETADATADB_H
#define CALI_CALIPERMETADATADB_H

#include <Attribute.h>
#include <RecordMap.h>

#include <map>
#include <memory>

namespace cali
{

class Node;

typedef std::map<cali_id_t, cali_id_t> IdMap;

class CaliperMetadataDB
{
    struct CaliperMetadataDBImpl;
    std::unique_ptr<CaliperMetadataDBImpl> mP;

public:

    CaliperMetadataDB();

    ~CaliperMetadataDB();

    //
    // --- I/O API 
    // 

    bool        read(const char* filename);

    RecordMap   merge(const RecordMap& rec, IdMap& map);


    //
    // --- Query API
    //

    const Node* node(cali_id_t id) const;
    Attribute   attribute(cali_id_t id);
};

}

#endif
