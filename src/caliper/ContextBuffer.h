///@ file ContextBuffer.h
///@ ContextBuffer class declaration

#ifndef CALI_CONTEXTBUFFER_H
#define CALI_CONTEXTBUFFER_H

#include <Record.h>
#include <Variant.h>

#include <memory>

namespace cali
{

class Attribute;
class Node;

template<int> class FixedSnapshot;
typedef FixedSnapshot<64> Snapshot;

class ContextBuffer
{
    struct ContextBufferImpl;

    std::unique_ptr<ContextBufferImpl> mP;

public:

    ContextBuffer();
    ~ContextBuffer();

    /// @name set / unset entries
    /// @{

    Variant  get(const Attribute&) const;
    Node*    get_node(const Attribute&) const;

    Variant  exchange(const Attribute&, const Variant&);

    cali_err set_node(const Attribute&, Node*);
    cali_err set(const Attribute&, const Variant&);
    cali_err unset(const Attribute&);

    /// @}
    /// @name get context
    /// @{

    void     snapshot(Snapshot* sbuf) const;

    /// @}
    /// @name Serialization API
    /// @{

    void     push_record(WriteRecordFn fn) const;

    /// @}
};

} // namespace cali

#endif // CALI_CONTEXTBUFFER_H
