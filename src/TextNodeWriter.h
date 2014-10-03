/// @file TextNodeWriter
/// TextNodeWriter class declaration

#include "Writer.h"

#include <memory>


namespace cali
{

class Query;

class TextNodeWriter : public NodeWriter 
{   
    struct TextNodeWriterImpl;

    std::unique_ptr<TextNodeWriterImpl> mP;

public:

    TextNodeWriter(std::ostream& os);

    ~TextNodeWriter();

    void write(const NodeWriter::NodeInfo& info, const Query& q) override;
};

}
