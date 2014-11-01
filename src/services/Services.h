/// @file Services.h
/// Services class declaration

#ifndef CALI_SERVICES_H
#define CALI_SERVICES_H

#include <memory>

namespace cali
{

class MetadataWriter;

class Services
{
    struct ServicesImpl;
    std::unique_ptr<ServicesImpl> mP;

public:

    struct MetadataWriterService {
        const char*   name;
        void                           (*register_fn)();
        std::unique_ptr<MetadataWriter>(*create_fn)();
    };

    static void register_services();

    static std::unique_ptr<MetadataWriter> get_metadata_writer(const char* name);
};

} // namespace cali

#endif // CALI_SERVICES_H
