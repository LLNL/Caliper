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
    // Create an annotation object for the "phase" annotation
    cali::Annotation phase_annotation("phase");

    // Declare begin of phase "main" 
    phase_annotation.begin("main");

    // Declare begin of phase "init" under "main". Thus, "phase" is now set to "main/init"
    phase_annotation.begin("init");
    int count = 4;
    // End innermost level of the phase annotation. Thus, "phase" is now set to "main"
    phase_annotation.end();

    if (count > 0) {
        // Declare begin of phase "loop" under "main". Thus, phase is now set to "main/loop".
        // The Guard object automatically closes this annotation level at the end
        // of the current C++ scope.
        cali::Annotation::Guard ann_loop( phase_annotation.begin("loop") );

        // Create iteration annotation object
        // The CALI_ATTR_ASVALUE option indicates that iteration values should be stored 
        // explicitly in each context record. 
        cali::Annotation iteration_annotation("iteration", CALI_ATTR_ASVALUE);
        
        for (int i = 0; i < count; ++i) {
            // Set "iteration" annotation to current value of 'i'
            iteration_annotation.set(i);
        }

        // Explicitly end the iteration annotation. This removes the iteration values 
        // from context records from here on. 
        iteration_annotation.end();

        // The ann_loop Guard object implicitly closes the "loop" annotation level here, 
        // thus phase is now back to "main"
    }

    // Close level "main" of the phase annotation, thus phase is now removed from context records
    phase_annotation.end();
}
