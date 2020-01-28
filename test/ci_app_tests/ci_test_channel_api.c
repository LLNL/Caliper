#include <caliper/cali.h>


int main()
{
    const char* channel_cfg[][2] = {
        { "CALI_CHANNEL_FLUSH_ON_EXIT", "false" },
        { NULL, NULL }
    };

    cali_configset_t cfg = cali_create_configset(channel_cfg);

    cali_configset_set(cfg, "CALI_SERVICES_ENABLE", "event,trace,recorder");
    cali_configset_set(cfg, "CALI_RECORDER_FILENAME", "stdout");
    
    cali_id_t chn_a = cali_create_channel("channel.a", 0, cfg);
    cali_id_t chn_b = cali_create_channel("channel.b", 0, cfg);
    
    cali_delete_configset(cfg);

    CALI_MARK_BEGIN("foo");

    cali_set_int_byname("b", 4);

    cali_deactivate_channel(chn_b);
    cali_set_int_byname("a", 2);
    cali_end_byname("a");
    cali_activate_channel(chn_b);
    
    cali_deactivate_channel(chn_a);
    cali_set_int_byname("c", 8);
    cali_end_byname("c");
    cali_activate_channel(chn_a);

    CALI_MARK_END("foo");

    cali_channel_flush(chn_a, 0);
    cali_channel_flush(chn_b, 0);

    cali_delete_channel(chn_a);
    cali_delete_channel(chn_b);

    return 0;
}
