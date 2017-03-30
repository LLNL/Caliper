/*
 * Caliper snapshot processing example
 */

#include <caliper/cali.h>

#include <stdio.h>

struct list_elem {
    cali_id_t      attr_id;
    const char*    attr_name;
    cali_variant_t val;
    list_elem*     next;
}

int append_to_list(void* arg, cali_id_t attr_id, cali_variant_t val)
{
    struct list_elem* elem = malloc(sizeof(struct list_elem));

    elem.attr_id = attr_id;
    elem.attr_name = cali_attribute_name(attr_id);
    elem.val = val;

    struct list_elem** ptr = (struct list_elem**) arg;
    
    elem.next = *ptr;
    *ptr = elem;

    return 0; /* continue unpacking */ 
}


int main()
{
    unsigned char buf[256];
    size_t snap_len = 0;

    snap_len = cali_sigsafe_pull_snapshot(CALI_SCOPE_THREAD, 256, buf,
                                          0, NULL, NULL, NULL);

    if (snap_len > 255)
        fprintf(stderr, "Insufficient snapshot buffer space, contents cut\n");
    
    if (snap_len > 0) {
        struct list_elem* list = NULL;

        cali_unpack_snapshot(buf, append_to_list, &list);
    }
}
