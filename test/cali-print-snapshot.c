/* *********************************************************************************************
 * Copyright (c) 2017, Lawrence Livermore National Security, LLC.  
 * Produced at the Lawrence Livermore National Laboratory.
 *
 * This file is part of Caliper.
 * Written by David Boehme, boehme3@llnl.gov.
 * LLNL-CODE-678900
 * All rights reserved.
 *
 * For details, see https://github.com/scalability-llnl/Caliper.
 * Please also see the LICENSE file for our additional BSD notice.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the disclaimer below.
 *  * Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions and the disclaimer (as noted below) in the documentation and/or other materials
 *    provided with the distribution.
 *  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * *********************************************************************************************/

#include "cali.h"

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
snapshot()
{
    /*
     * Take a snapshot, and store it in our buffer
     */

    unsigned char buffer[80];        
    size_t len = cali_pull_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, 80, buffer);

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
do_work()
{
    CALI_MARK_FUNCTION_BEGIN;
    CALI_MARK_LOOP_BEGIN(loopmarker, "foo");

    for (int i = 0; i < 2; ++i) {
        CALI_MARK_ITERATION_BEGIN(loopmarker, i);

        snapshot();

        CALI_MARK_ITERATION_END(loopmarker);
    }

    CALI_MARK_LOOP_END(loopmarker);
    CALI_MARK_FUNCTION_END;
}


int main()
{
    CALI_MARK_FUNCTION_BEGIN;

    do_work();

    CALI_MARK_FUNCTION_END;
}
