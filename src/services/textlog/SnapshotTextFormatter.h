/// \file  SnapshotTextFormatter.h
/// \brief Interface for Snapshot text formatter

#ifndef CALI_SNAPSHOT_TEXT_FORMATTER_H
#define CALI_SNAPSHOT_TEXT_FORMATTER_H

#include <Caliper.h>

#include <Attribute.h>

#include <iostream>
#include <memory>
#include <string>

namespace cali
{

class SnapshotTextFormatter
{
    struct SnapshotTextFormatterImpl;

    std::unique_ptr<SnapshotTextFormatterImpl> mP;

public:

    SnapshotTextFormatter();

    ~SnapshotTextFormatter();

    void parse(const std::string& format_str, Caliper* c);

    void update_attribute(const Attribute& attr);

    void print(std::ostream& os, const Snapshot* s) const;
};

}

#endif
