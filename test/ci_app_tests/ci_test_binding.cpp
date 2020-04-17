// Test for annotation binding interface 

#include "caliper/AnnotationBinding.h"

#include "caliper/cali.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace cali;

class TestBinding : public AnnotationBinding
{
    Attribute   m_my_attr;
    Attribute   m_prop_attr;
    static bool s_verbose;

public:

    const char* service_tag() const { 
        return "testbinding"; 
    }

    void initialize(Caliper* c, Channel*) {
        m_my_attr = 
            c->create_attribute("testbinding",  CALI_TYPE_STRING, CALI_ATTR_NOMERGE);
        m_prop_attr =
            c->create_attribute("testproperty", CALI_TYPE_INT,    CALI_ATTR_DEFAULT);
    }

    void on_mark_attribute(Caliper* c, Channel* chn, const Attribute& attr) {
        if (s_verbose)
            std::cout << "TestBinding::on_mark_attribute(" << attr.name() << ")" << std::endl;

        c->make_tree_entry(m_prop_attr, Variant(4242), c->node(attr.id()));
    }

    void on_begin(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value) {
        if (attr == m_my_attr)
            return;

        // assert(attr.get(m_prop_attr) == Variant(4242));

        std::string s(attr.name());
        s.append("=").append(value.to_string());
        
        c->begin(m_my_attr, Variant(CALI_TYPE_STRING, s.c_str(), s.size()));

        if (s_verbose)
            std::cout << "begin " << s << std::endl;
    }

    void on_end(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value) {
        if (attr == m_my_attr)
            return;

        c->end(m_my_attr);

        if (s_verbose)
            std::cout << "end   " << attr.name() << "=" << value.to_string() << std::endl;
    }

    static void set_verbose(bool v) {
        s_verbose = v;
    }
};

bool TestBinding::s_verbose = false;


int main(int argc, const char** argv)
{
    if (argc > 1 && 0 == strcmp(argv[1], "--verbose"))
        TestBinding::set_verbose(true);

    Caliper c;

    AnnotationBinding::make_binding<TestBinding>(&c, c.get_channel(0));

    Attribute nested_attr  =
        c.create_attribute("binding.nested",  CALI_TYPE_STRING, CALI_ATTR_NESTED);
    Attribute default_attr =
        c.create_attribute("binding.default", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);

    c.begin(nested_attr,  Variant(CALI_TYPE_STRING, "outer",   6));
    c.begin(nested_attr,  Variant(CALI_TYPE_STRING, "inner",   6));
    c.begin(default_attr, Variant(CALI_TYPE_STRING, "default", 8));
    c.end(default_attr);
    c.end(nested_attr);
    c.end(nested_attr);
}
