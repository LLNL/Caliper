Variorum Service
================

The Variorum service implements a monitoring service of energy usage at runtime.
It implements the `snapshot` callback for adding data to Caliper snapshot
records. Minimum Variorum version is v0.8.
Nesting of Caliper markers is not supported yet in this service.

To build with the Variorum service, set WITH_VARIORUM=On and 
VARIORUM_PREFIX=<path-to-variorum-install>.

With the following configuration, Caliper will export its
data to a cali file:

```
    $ CALI_SERVICES_ENABLE=aggregate,event,variorum,recorder \
      CALI_VARIORUM_DOMAINS=energy_node_joules \
```

