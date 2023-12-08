# LDMS Connector/Forwarder

The LDMS Connector/Fowarder publishes Caliper messages to LDMS for analysis and visualization. It requires LDMS to be installed. You can find the current LDMS installation here: [OVIS](https://github.com/ovis-hpc/ovis)

- Installation guidelines can be found here: [LDMS Quickstart Guide](https://ovis-hpc.readthedocs.io/en/latest/ldms/ldms-quickstart.html)
- The following environmental variables are required to successfully publish Caliper messages to LDMS:
  ```
  export CALIPER_LDMS_STREAM="caliper-perf-data"
  export CALIPER_LDMS_XPRT="sock"
  export CALIPER_LDMS_HOST="localhost"
  export CALIPER_LDMS_PORT="412"
  export CALIPER_LDMS_AUTH="munge"
  export LD_LIBRARY_PATH="[path_to_LDMS_lib64]":$LD_LIBRARY_PATH
  export PATH="[path_to_LDMS_bin_directory]":"[path_to_LDMS_sbin_directory]":$PATH
  ```
- The following environmental variable allows you to debug your LDMS connection and confirm if messages are being pushed by the application and Caliper:
  ```
  export CALIPER_LDMS_VERBOSE = 1
  ```
  Set ```CALIPER_LDMS_VERBOSE = 0``` to turn off debug messages.
