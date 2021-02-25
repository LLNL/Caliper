// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/ChannelController.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include "caliper/Caliper.h"

using namespace cali;

struct ChannelController::ChannelControllerImpl
{
    std::string  name;
    int          flags;
    config_map_t config;

    Channel*     channel = nullptr;

    ChannelControllerImpl(const char* cname, int cflags, const config_map_t& cfg)
        : name(cname),
          flags(cflags),
          config(cfg)
    { }

    ~ChannelControllerImpl() {
        if (channel) {
            Caliper c;
            c.delete_channel(channel);
        }
    }
};

Channel*
ChannelController::channel()
{
    return mP->channel;
}

config_map_t&
ChannelController::config()
{
    return mP->config;
}

config_map_t
ChannelController::copy_config() const
{
    return mP->config;
}

Channel*
ChannelController::create()
{
    if (mP->channel)
        return mP->channel;

    RuntimeConfig cfg;

    cfg.allow_read_env(mP->flags & CALI_CHANNEL_ALLOW_READ_ENV);
    cfg.import(mP->config);

    Caliper c;

    mP->channel = c.create_channel(mP->name.c_str(), cfg);

    if (!mP->channel) {
        Log(0).stream() << "ChannelController::create(): Could not create channel "
                        << mP->name << std::endl;
        return nullptr;
    }

    if (mP->flags & CALI_CHANNEL_LEAVE_INACTIVE)
        c.deactivate_channel(mP->channel);

    on_create(&c, mP->channel);

    //   Reset the object's channel pointer if the channel is destroyed
    // behind our back (e.g., in Caliper::release())
    mP->channel->events().finish_evt.connect([this](Caliper*, Channel*){
            mP->channel = nullptr;
        });

    return mP->channel;
}

void
ChannelController::start()
{
    Caliper  c;
    Channel* chn = mP->channel;

    if (!chn)
        chn = create();
    if (chn)
        c.activate_channel(chn);
}

void
ChannelController::stop()
{
    if (mP->channel)
        Caliper().deactivate_channel(mP->channel);
}

bool
ChannelController::is_active() const
{
    return mP->channel && mP->channel->is_active();
}

void
ChannelController::flush()
{
    Channel* chn = channel();

    if (chn)
        Caliper().flush_and_write(chn, nullptr);
}

std::string
ChannelController::name() const
{
    return mP->name;
}

ChannelController::ChannelController(const char* name, int flags, const config_map_t& cfg)
    : mP { new ChannelControllerImpl(name, flags, cfg) }
{
}

ChannelController::~ChannelController()
{
}
