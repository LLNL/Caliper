# Copyright (c) 2024, Lawrence Livermore National Security, LLC.
# See top-level LICENSE file for details.

from pycaliper.high_level import annotate_function
from pycaliper.config_manager import ConfigManager
from pycaliper.instrumentation import (
    set_global_byname,
    begin_region,
    end_region,
)
from pycaliper.loop import Loop

import argparse
import sys
import time


def get_available_specs_doc(mgr: ConfigManager):
    doc = ""
    for cfg in mgr.available_config_specs():
        doc += mgr.get_documentation_for_spec(cfg)
        doc += "\n"
    return doc


@annotate_function()
def foo(i: int) -> float:
    nsecs = max(i * 500, 100000)
    secs = nsecs / 10**9
    time.sleep(secs)
    return 0.5 * i


def main():
    mgr = ConfigManager()

    parser = argparse.ArgumentParser()
    parser.add_argument("--caliper_config", "-P", type=str, default="",
                        help="Configuration for Caliper\n{}".format(get_available_specs_doc(mgr)))
    parser.add_argument("iterations", type=int, nargs="?", default=4,
                        help="Number of iterations")
    args = parser.parse_args()
    
    mgr.add(args.caliper_config)

    if mgr.error():
        print("Caliper config error:", mgr, file=sys.stderr)

    mgr.start()

    set_global_byname("iterations", args.iterations)
    set_global_byname("caliper.config", args.caliper_config)
    
    begin_region("main")
    
    begin_region("init")
    t = 0
    end_region("init")
    
    loop_ann = Loop("mainloop")
    
    for i in range(args.iterations):
        loop_ann.start_iteration(i)
        t *= foo(i)
        loop_ann.end_iteration()

    loop_ann.end()

    end_region("main")
    
    mgr.flush()
    

if __name__ == "__main__":
    main()
    