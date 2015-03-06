///@file Args.h
///Argument parsing 

#ifndef UTIL_ARGS_H
#define UTIL_ARGS_H

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace util
{

class Args {
    struct ArgsImpl;
    std::shared_ptr<ArgsImpl> mP;

public:

    struct Table {
        const char* name;

        const char* longopt;
        char        shortopt;

        bool        has_argument;

        const char* info;
        const char* argument_info;
      
        static const Table Terminator;
    };

    Args();

    Args(const Table table[]);

    ~Args();

    bool set_fail(bool fail = true);
    void add_table(const Table table[]);

    // --- Parse

    /// Parses command-line options given by @arg argc and @argv.
    /// @return @arg argv[] index of first unknown option argument
    int  parse(int argc, const char* argv[], int pos = 1);

    // --- Retrieval

    std::string program_name() const;

    std::string get(const std::string& name, const std::string& def = "") const;
    bool        is_set(const std::string& name) const;

    std::vector<std::string> options() const;
    std::vector<std::string> arguments() const;

    // --- Info 

    void print_available_options(std::ostream& os) const;
};

}

#endif
