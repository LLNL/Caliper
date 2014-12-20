/// @file CaliperMetadataDB
/// CaliperMetadataDB class declaration

#ifndef CALI_CALIPERMETADATADB_H
#define CALI_CALIPERMETADATADB_H

#include <Attribute.h>

#include <memory>

namespace cali
{

class Node;

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

    void read(const char* filename);

    //
    // --- Query API
    //

    const Node* node(cali_id_t id) const;
    Attribute   attribute(cali_id_t id);
};

}

#endif
