// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/ChannelController.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include "caliper/Caliper.h"

using namespace cali;

struct ChannelController::ChannelControllerImpl {
    std::string  name;
    int          flags;
    config_map_t config;
    info_map_t   metadata;

    Channel channel;

    ChannelControllerImpl(const char* cname, int cflags, const config_map_t& cfg)
        : name(cname), flags(cflags), config(cfg)
    {}

    ~ChannelControllerImpl()
    {
        if (channel) {
            Caliper c;
            c.delete_channel(channel);
        }
    }
};

namespace
{

void add_channel_metadata(Caliper& c, Channel& channel, const info_map_t& metadata)
{
    for (const auto& entry : metadata) {
        auto attr = c.create_attribute(
            entry.first,
            CALI_TYPE_STRING,
            CALI_ATTR_GLOBAL | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_UNALIGNED
        );

        c.set(channel.body(), attr, Variant(entry.second.c_str()));
    }
}

} // namespace

Channel ChannelController::channel()
{
    return mP->channel;
}

ChannelBody* ChannelController::channel_body()
{
    return mP->channel.body();
}

config_map_t& ChannelController::config()
{
    return mP->config;
}

config_map_t ChannelController::copy_config() const
{
    return mP->config;
}

info_map_t& ChannelController::metadata()
{
    return mP->metadata;
}

Channel ChannelController::create()
{
    if (mP->channel)
        return mP->channel;

    RuntimeConfig cfg;

    cfg.allow_read_env(mP->flags & CALI_CHANNEL_ALLOW_READ_ENV);
    cfg.import(mP->config);

    Caliper c;

    mP->channel = c.create_channel(mP->name.c_str(), cfg);

    if (!mP->channel) {
        Log(0).stream() << "ChannelController::create(): Could not create channel " << mP->name << std::endl;
        return Channel();
    }

    on_create(&c, mP->channel);
    add_channel_metadata(c, mP->channel, mP->metadata);

    //   Reset the object's channel pointer if the channel is destroyed
    // behind our back (e.g., in Caliper::release())
    mP->channel.events().finish_evt.connect([this](Caliper*, Channel*) { mP->channel = Channel(); });

    return mP->channel;
}

void ChannelController::start()
{
    Caliper c;

    if (!mP->channel)
        create();
    if (mP->channel)
        c.activate_channel(mP->channel);
}

void ChannelController::stop()
{
    if (mP->channel)
        Caliper().deactivate_channel(mP->channel);
}

bool ChannelController::is_active() const
{
    return mP->channel && mP->channel.is_active();
}

bool ChannelController::is_instantiated() const
{
    return mP && mP->channel;
}

void ChannelController::flush()
{
    Channel chn = channel();
    if (chn)
        Caliper().flush_and_write(chn.body(), SnapshotView());
}

std::string ChannelController::name() const
{
    return mP->name;
}

ChannelController::ChannelController(const char* name, int flags, const config_map_t& cfg)
    : mP { new ChannelControllerImpl(name, flags, cfg) }
{}

ChannelController::~ChannelController()
{}
