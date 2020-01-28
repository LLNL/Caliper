/* Copyright (c) 2019, Lawrence Livermore National Security, LLC.
 * See top-level LICENSE file for details.
 */

#include <caliper/cali.h>

#include <stdio.h>
#include <string.h>

/* This callback function will be called for each snapshot element
 * from cali_unpack_snapshot().
 * Elements with the same attribute key will appear in top-down order.
 */
int print_entry(void* user_arg, cali_id_t attr_id, cali_variant_t val)
{
    int* counter = (int*) user_arg;

    if ((*counter)++ > 0)
        printf(", ");

    const char* attr_name = cali_attribute_name(attr_id);

    if (attr_name)
        printf("%s=", attr_name);
    else {
        printf("(Unknown)");
        return 1;
    }

    cali_attr_type type = cali_variant_get_type(val);

    switch (type) {
    case CALI_TYPE_INT:
    case CALI_TYPE_UINT:
    case CALI_TYPE_BOOL:
        printf("%d", cali_variant_to_int(val, NULL));
        break;
    case CALI_TYPE_STRING:
        {
            char   buf[20];
            size_t len = cali_variant_get_size(val);
            len = (len < 19 ? len : 19);

            strncpy(buf, (char*) cali_variant_get_data(&val), len);
            buf[len] = '\0';

            printf("\"%s\"", buf);
        }
        break;
    default:
        printf("<type %s not supported>", cali_type2string(type));
    }

    return 1; /* Non-null return value: keep processing. */
}


/*
 * Take and process a snapshot
 */
void
snapshot(cali_id_t channel)
{
    /*
     * Take a snapshot, and store it in our buffer
     */

    unsigned char buffer[80];
    size_t len = cali_channel_pull_snapshot(channel, CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, 80, buffer);

    if (len == 0) {
        fprintf(stderr, "Could not obtain snapshot!\n");
        return;
    }
    if (len > 80) {
        /* Our buffer is too small (very unlikely) */
        fprintf(stderr, "Snapshot buffer too small! Need %lu bytes.\n", len);
        return;
    }

    size_t bytes_read = 0;

    /*
     * Unpack the snapshot, and print the elements using our callback function
     */

    int counter = 0;

    printf("Snapshot: { ");
    cali_unpack_snapshot(buffer, &bytes_read, print_entry, &counter);
    printf(" }. %lu bytes, %d entries.\n", bytes_read, counter);
}


/*
 * Example function: add some annotations and take snapshots
 */
void
do_work(cali_id_t channel)
{
    CALI_MARK_FUNCTION_BEGIN;
    CALI_MARK_LOOP_BEGIN(loopmarker, "foo");

    for (int i = 0; i < 2; ++i) {
        CALI_MARK_ITERATION_BEGIN(loopmarker, i);

        snapshot(channel);

        CALI_MARK_ITERATION_END(loopmarker);
    }

    CALI_MARK_LOOP_END(loopmarker);
    CALI_MARK_FUNCTION_END;
}


int main()
{
    const char* cfg[][2] = { { NULL, NULL } };
    cali_configset_t cfgset = cali_create_configset(cfg);
    cali_id_t channel = cali_create_channel("print-snapshot", 0, cfgset);
    cali_delete_configset(cfgset);

    CALI_MARK_FUNCTION_BEGIN;

    do_work(channel);

    CALI_MARK_FUNCTION_END;

    cali_delete_channel(channel);
}
