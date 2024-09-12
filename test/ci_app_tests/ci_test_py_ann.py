# --- Caliper continuous integration test app for Python annotation interface

import sys

sys.path.insert(0, "@PYPATH_TESTING@")

from pycaliper import config_preset
from pycaliper.instrumentation import (
    Attribute,
    set_global_byname,
    begin_byname,
    set_byname,
    end_byname,
)
from pycaliper.types import CALI_TYPE_INT, CALI_ATTR_ASVALUE, CALI_TYPE_STRING, CALI_ATTR_UNALIGNED
from pycaliper.variant import Variant
from pycaliper.config_manager import ConfigManager


def main():
    config_preset({"CALI_CHANNEL_FLUSH_ON_EXIT": "false"})

    mgr = ConfigManager()
    if len(sys.argv) > 1:
        mgr.add(sys.argv[1])

    if mgr.error():
        print("Caliper config error:", mgr.err_msg(), file=sys.stderr)
        exit(-1)

    mgr.start()

    set_global_byname("global.double", 42.42)
    set_global_byname("global.int", 1337)
    set_global_byname("global.string", "my global string")
    set_global_byname("global.uint", 42)

    iter_attr = Attribute("iteration", CALI_TYPE_INT, CALI_ATTR_ASVALUE)

    begin_byname("phase", "loop")

    for i in range(4):
        iter_attr.begin(i)
        iter_attr.end()

    end_byname("phase")

    begin_byname("ci_test_c_ann.meta-attr")

    meta_attr = Attribute("meta-attr", CALI_TYPE_INT)
    meta_val = Variant(47)

    test_attr = Attribute(
        "test-attr-with-metadata",
        CALI_TYPE_STRING,
        CALI_ATTR_UNALIGNED,
        [meta_attr],
        [meta_val],
    )

    test_attr.set("abracadabra")

    end_byname("ci_test_c_ann.meta-attr")

    begin_byname("ci_test_c_ann.setbyname")

    set_byname("attr.int", 20)
    set_byname("attr.dbl", 1.25)
    set_byname("attr.str", "fidibus")

    end_byname("ci_test_c_ann.setbyname")

    mgr.flush()


if __name__ == "__main__":
    main()
