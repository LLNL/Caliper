// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// A basic Caliper instrumentation demo / test file

#include <Annotation.h>
#include <Caliper.h>

#include <Variant.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

void begin_foo_op()
{
    // Begin "foo"->"fooing" and keep it alive past the end of the current C++ scope
    cali::Annotation("foo").begin("fooing");
}

void end_foo_op()
{
    // Explicitly end innermost level of "foo" annotation
    cali::Annotation("foo").end();
}

void make_hierarchy_1()
{
    cali::Caliper   c;    
    cali::Attribute attr = c.create_attribute("misc.hierarchy", CALI_TYPE_STRING);

    cali::Variant   data[3] = {
        { CALI_TYPE_STRING, "h1_l0", 5 },
        { CALI_TYPE_STRING, "h1_l1", 5 },
        { CALI_TYPE_STRING, "h1_l2", 5 }
    };

    c.set_path(attr, 3, data);
}

void make_hierarchy_2()
{
    cali::Caliper   c;
    cali::Attribute attr = c.create_attribute("misc.hierarchy", CALI_TYPE_STRING);

    cali::Variant   data[3] = {
        { CALI_TYPE_STRING, "h2_l0", 5 },
        { CALI_TYPE_STRING, "h2_l1", 5 },
        { CALI_TYPE_STRING, "h2_l2", 5 }
    };

    c.set_path(attr, 3, data);
}

int main(int argc, char* argv[])
{
    // Declare "phase" annotation
    cali::Annotation phase("phase");

    // Begin scope of phase->"main"
    cali::Annotation::Guard ann_main( phase.begin("main") );

    int count = argc > 1 ? atoi(argv[1]) : 4;

    // An annotation with a user-defined datatype

    struct my_weird_type {
        unsigned u = 42;
        char     c = 'c';
        float    f = 42.42;
    } my_weird_elem;

    cali::Annotation::Guard 
        g_mydata( cali::Annotation("mydata").set(CALI_TYPE_USR, &my_weird_elem, sizeof(my_weird_elem)) );

    // A hierarchy to test the Caliper::set_path() API call
    make_hierarchy_1();

    {
        // Add new scope phase->"loop" under phase->"main"
        // Annotation::Guard used to be AutoScope: keep backward compatibility
        cali::Annotation::AutoScope
            g_loop( phase.begin("loop") );

        // Set "loopcount" annotation to 'count' in current C++ scope
        cali::Annotation::Guard 
            g_loopcount( cali::Annotation("loopcount").set(count) );

        // Declare "iteration" annotation, store entries explicitly as values
        cali::Annotation iteration("iteration", CALI_ATTR_ASVALUE);

        cali::Annotation::Guard
            g_iteration(iteration);

        for (int i = 0; i < count; ++i) {
            // Set "iteration" annotation to current value of 'i'
            iteration.set(i);

            begin_foo_op();
            end_foo_op();
        }

        // "loop", "loopcount" and "iteration" annotations implicitly end here 
    }

    {
        phase.begin("finalize");

        // A different hierarchy to test the Caliper::set_path() API call
        make_hierarchy_2();

        phase.end();
    }

    // implicitly end phase->"main"
}
