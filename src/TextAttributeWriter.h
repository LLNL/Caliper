/// @file TextAttributeWriter
/// TextAttributeWriter class declaration

#include "Writer.h"

#include <memory>


namespace cali
{

class Attribute;

class TextAttributeWriter : public AttributeWriter 
{   
    struct TextAttributeWriterImpl;

    std::unique_ptr<TextAttributeWriterImpl> mP;

public:

    TextAttributeWriter(std::ostream& os);

    ~TextAttributeWriter();

    void write(const Attribute&) override;
};

}
