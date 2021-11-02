Variorum Service
================

The Variorum service implements a monitoring service of power usage at runtime.
It implements the `snapshot` callback for adding data to Caliper snapshot
records.

With the following configuration, Caliper will export its
data to a cali file:

```
    $ CALI_SERVICES_ENABLE=aggregate,event,variorum,recorder \
      CALI_VARIORUM_DOMAINS=power_node_watts \
```
