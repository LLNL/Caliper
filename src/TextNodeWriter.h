/// @file TextNodeWriter
/// TextNodeWriter class declaration

#include "Writer.h"

#include <memory>


namespace cali
{

class NodeQuery;

class TextNodeWriter : public NodeWriter 
{   
    struct TextNodeWriterImpl;

    std::unique_ptr<TextNodeWriterImpl> mP;

public:

    TextNodeWriter(std::ostream& os);

    ~TextNodeWriter();

    void write(const NodeQuery&) override;
};

}
