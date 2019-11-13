// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

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
