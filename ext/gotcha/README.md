GOTCHA v1.0.2
============

Gotcha is a library that wraps functions.  Tools can use gotcha to install hooks into other libraries, for example putting a wrapper function around libc's malloc.  
It is similar to LD_PRELOAD, but operates via a programable API.
This enables easy methods of accomplishing tasks like code instrumentation or wholesale replacement of mechanisms in programs
without disrupting their source code.

Note
-----------

This copy of GOTCHA v1.0.2 is shipped with Caliper.
Find the main GOTCHA repository on [Github](https://github.com/llnl/GOTCHA).

Quick Start
-----------

*Building Gotcha* is trivial. In the root directory of the repo

```
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX = <where you want the sofware> ..
make install
```
*Usage* is fairly simple. For us to wrap a function, we need to know its name, what you want it wrapped with (the wrapper), and we need to give you some ability to call the function you wrapped (wrappee). Gotcha works on triplets containing this information. We have [small sample uses](src/example/autotee/autotee.c), but the standard workflow looks like


```
  #include <gotcha/gotcha.h>
  static gotcha_wrappee_t wrappee_puts_handle;
  static int puts_wrapper(const char* str); //this is the declaration of your wrapper
  static gotcha_wrappee_t wrappee_fputs_handle;
  static int fputs_wrapper(const char* str, FILE* f);
  struct gotcha_binding_t wrap_actions [] = {
    { "puts", puts_wrapper, &wrappee_puts_handle },
    { "fputs", fputs_wrapper, &wrappee_fputs_handle },
  };
  int init_mytool(){
    gotcha_wrap(wrap_actions, sizeof(wrap_actions)/sizeof(struct gotcha_binding_t), "my_tool_name");
  }
  static int fputs_wrapper(const char* str, FILE* f){
    // insert clever tool logic here
    typeof(&fputs_wrapper) wrappee_fputs = gotcha_get_wrappee(wrappee_fputs_handle); // get my wrappee from Gotcha
    return wrappee_fputs(str, f); //wrappee_fputs was directed to the original fputs by gotcha_wrap
  }

```

*Building your tool* changes little, you just need to add the prefix you installed Gotcha to your include directories, the location
the library was installed (default is <that_prefix>/lib) to your library search directories (-L...), and link
libgotcha.so (-lgotcha) with your tool. Very often this becomes "add -lgotcha to your link line," and nicer CMake integration is coming down the pipe.

A more advanced example can be seen in the [GOTCHA-tracer](https://github.com/llnl/GOTCHA-tracer) project.
This example shows how to implement a simple tracer with GOTCHA, including linking to GOTCHA through CMake and testing GOTCHA through direct invocation and using `LD_PRELOAD`.

That should represent all the work your application needs to do to use Gotcha.

Contact/Legal
-----------

The license is [LGPL](LGPL).

Primary contact/Lead developer

David Poliakoff (poliakoff1@llnl.gov)

Other developers

Matt Legendre  (legendre1@llnl.gov)
