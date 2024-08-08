import sys
import sysconfig

if len(sys.argv) != 3 or sys.argv[1] not in ("purelib", "platlib"):
    raise RuntimeError(
        "Usage: python get_python_install_paths.py <purelib | platlib> <sysconfig_scheme>"
    )

install_dir = sysconfig.get_path(sys.argv[1], sys.argv[2], {"userbase": "", "base": ""})

if install_dir.startswith("/"):
    install_dir = install_dir[1:]

print(install_dir, end="")
