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

// A minimal Caliper instrumentation demo 

#include <Annotation.h>


void foo(int count)
{
    // Mark "foo" kernel. Scope guard automatically clears 'foo' on function exit
    
    cali::Annotation::Guard
        g_phase( cali::Annotation("cali-demo.foo").begin() );

    { 
        // Create "iteration" annotation for iteration counter.
        // To avoid adding nodes for each iteration to the context tree,
        // we use the CALI_ATTR_ASVALUE flag to store explicit 'key:value' pairs
        // for iteration attributes on the blackboard

        cali::Annotation
            iter_ann("iteration", CALI_ATTR_ASVALUE);

        // Guard for iter_ann will automatically clear the last 'iteration' when
        // leaving the scope

        cali::Annotation::Guard
            g_iter( iter_ann );

        for (int i = 0; i < count; ++i)
            // set 'iteration:<i>'
            // Unlike begin(), set() overwrites the attribute's current value
            // rather than appending a new one
            iter_ann.set(i); 
    }
}

int main(int argc, char* argv[])
{
    // Mark main program. The scope guard will automatically clear it
    // on function exit
    
    cali::Annotation::Guard
        g_main( cali::Annotation("cali-demo.main").begin() );

    // Mark initialization phase

    cali::Annotation init_ann( cali::Annotation("cali-demo.init").begin() );
    int count = 4;
    init_ann.end();
    
    // Call kernel
    foo(count);

    // Implicitly clear main program annotation
    return 0;
}
