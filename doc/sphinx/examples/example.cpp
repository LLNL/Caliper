// An example program for Caliper functionality //

#include <caliper/Annotation.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
using namespace std;

int factorial(int n);

int main(int argc, char* argv[]) {
  // Mark begin of "initialization" phase
  cali::Annotation main_attr = cali::Annotation("main").begin("init");

  // Initialize program
  int count = 5;

  // Mark end of "initilization" phase
  main_attr.set("body");

  if(count > 0) {

    // Mark begin of "init" phase in the body
    main_attr.begin("init");

    int a = 0;
    int b = 0;

    // Set phase to "loop"
    main_attr.set("loop");

    // Create "iteration" attribute to export the iteration count
    cali::Annotation iteration_attr("iteration");

    for (int i = 0; i < count; ++i) {

      // Export current iteration count under "iteration"
      iteration_attr.set(i);

      // Perform computation
      a = factorial(i);
      b += a;
    }

    // Clear the iteration attribute
    iteration_attr.end();

    // Change main to "conclusion"
    main_attr.set("conclusion");

    // Conclude the program
    cout << "b = " << b << endl;

    // End the main attribute
    main_attr.end();

    return 0;
  }
}


int factorial(int n) {
  // Create and set the factorial attribute to "init"
  cali::Annotation fact_attr = cali::Annotation("factorial").begin("init");

  // Initialize program
  int f = 1;

  // Set factorial attr to "comp"
  fact_attr.set("comp");

  // Perform computations
  if(n > 1) {
    f = factorial(n-1)*n;
  }

  // End factorial attr
  fact_attr.end();

  // End program
  return f;
}
