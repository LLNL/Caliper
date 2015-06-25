///@file RecordProcessor.h
///RecordProcessor declarations

#ifndef CALI_RECORDPROCESSOR_H
#define CALI_RECORDPROCESSOR_H

#include <RecordMap.h>

#include <functional>

namespace cali
{

class CaliperMetadataDB;

typedef std::function<void(CaliperMetadataDB& db,const RecordMap& rec)> RecordProcessFn;
typedef std::function<void(CaliperMetadataDB& db,const RecordMap& rec, RecordProcessFn)> RecordFilterFn;


} // namespace cali

#endif
