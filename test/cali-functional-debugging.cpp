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

#include <caliper/cali.h>

#include <caliper/Caliper.h>
#include <caliper/SnapshotRecord.h>

#include <caliper/common/RuntimeConfig.h>
#include <caliper/common/Variant.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>


#include <caliper/CaliFunctional.h>

struct dog{
   int num_dogs;
};

struct special_dog : public dog {
};

cali::Annotation int_annotator("int_value");
namespace cali{

cali::Annotation dog_annotator("dog");
template<>
struct Recordable<dog>{
  static void record(dog in){
     dog_annotator.begin(in.num_dogs);
     dog_annotator.end();
  }
};

template<>
struct Recordable<int>{
  static void record(int instance){
     int_annotator.begin(instance);
     int_annotator.end();
  }
};
} //end namespace cali

void original_adder(int* in, dog dog_instance){}
auto adder = cali::make_recorded_function("adder",original_adder);

int main(int argc, char* argv[])
{
    int doggo = 6;
    dog goodest;
    goodest.num_dogs = 9;
    adder(&doggo,goodest);
    return 0;
}
