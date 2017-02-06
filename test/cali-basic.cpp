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

int main(int argc, char* argv[])
{
    // Create annotation object for "phase" annotation
    cali::Annotation phase_ann("phase");

    // Mark begin of "initialization" phase
    phase_ann.begin("initialization");

    // perform initialization tasks
    int count = 4;
    double t = 0.0, delta_t = 1e-6;

    // Mark end of "initialization" phase and begin of "loop" phase
    phase_ann.end();
    phase_ann.begin("loop");

    // Create "iteration" attribute to export the iteration count
    cali::Annotation iteration_ann("iteration");
        
    for (int i = 0; i < count; ++i) {
        // Mark each loop iteration  
        // The Annotation::Guard object will automatically "end" 
        // the annotation at the end of the C++ scope
        cali::Annotation::Guard 
            g_iteration( iteration_ann.begin(i) );

        // A Caliper snapshot taken at this point will contain
        // { "loop", "iteration"=<i> }

        // perform computation
        t += delta_t;
    }

    // Mark end of "loop" phase
    phase_ann.end();
}
