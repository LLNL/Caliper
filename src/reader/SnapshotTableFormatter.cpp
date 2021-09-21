#include "SnapshotTableFormatter.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/Node.h"

#include "../common/util/format_util.h"

#include <iostream>

using namespace cali;

namespace
{

struct EntryInfo  {
    std::string key;
    std::string val;
    bool align_right;
};

struct RecordInfo {
    std::vector<EntryInfo> entries;
    size_t max_key_len;
    size_t max_right_len;

    void add(const CaliperMetadataAccessInterface& db, cali_id_t attr_id, const Variant& data) {
        Attribute attr = db.get_attribute(attr_id);

        bool align_right =
            attr.type() == CALI_TYPE_DOUBLE ||
            attr.type() == CALI_TYPE_INT    ||
            attr.type() == CALI_TYPE_UINT;

        EntryInfo info { attr.name(), data.to_string(), align_right };

        max_key_len = std::max(max_key_len, info.key.length());

        if (align_right)
            max_right_len = std::max(max_right_len, info.val.length());

        entries.push_back(info);
    }

    RecordInfo()
        : max_key_len { 0 }, max_right_len { 0 }
        { }
};

RecordInfo unpack_record(CaliperMetadataAccessInterface& db, const std::vector<Entry>& rec)
{
    RecordInfo info;

    for (const Entry& e : rec) {
        if (e.is_reference()) {
            for (const Node* node = e.node(); node && node->attribute() != CALI_INV_ID; node = node->parent())
                info.add(db, node->attribute(), node->data());
        } else if (e.is_immediate()) {
            info.add(db, e.attribute(), e.value());
        }
    }

    return info;
}

std::ostream& write_record_data(const RecordInfo& info, std::ostream& os)
{
    const size_t MAX_KEY_COL_WIDTH = 24;
    const size_t MAX_VAL_COL_WIDTH = 52;

    size_t key_width = std::min(info.max_key_len, MAX_KEY_COL_WIDTH);
    size_t val_width = MAX_VAL_COL_WIDTH;

    int count = 0;

    for (const EntryInfo& e : info.entries) {
        if (count++ > 0)
            os << "\n";

        util::pad_right(os, util::clamp_string(e.key, key_width), key_width) << ": ";

        if (e.align_right)
            util::pad_left(os, e.val, info.max_right_len);
        else
            os << util::clamp_string(e.val, val_width);
    }

    return os << "\n";
}

} // namespace anonymous

namespace cali
{

std::ostream& format_record_as_table(CaliperMetadataAccessInterface& db, const std::vector<Entry>& rec, std::ostream& os)
{
    return ::write_record_data(::unpack_record(db, rec), os);
}

}
