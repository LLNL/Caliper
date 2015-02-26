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

class ContextBuffer
{
    struct ContextBufferImpl;

    std::unique_ptr<ContextBufferImpl> mP;

    static const RecordDescriptor s_record;

public:

    ContextBuffer();
    ~ContextBuffer();

    /// @name set / unset entries
    /// @{

    Variant  get(const Attribute&) const;

    cali_err set(const Attribute&, const Variant&);
    cali_err unset(const Attribute&);

    /// @}
    /// @name get context
    /// @{

    size_t   pull_context(uint64_t* buf, size_t size) const;

    /// @}
    /// @name Serialization API
    /// @{

    void     push_record(WriteRecordFn fn) const;

    static const RecordDescriptor& record_descriptor() { return s_record; }

    /// @}
};

} // namespace cali

#endif // CALI_CONTEXTBUFFER_H
