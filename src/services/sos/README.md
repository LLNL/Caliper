SOS Service
--------------------------------

The SOS service publishes Caliper trace or aggregation data into SOS_flow. 
Data will be published on the end event for the attribute selected by 
CALI_SOS_TRIGGER_ATTR.

For example, with the following configuration, Caliper will export its
aggregation database after each iteration of cali-basic's main loop::

```
  $ CALI_SERVICES_ENABLE=aggregate:event:sos:timestamp \
      CALI_SOS_TRIGGER_ATTR="iteration#mainloop" \
      CALI_LOG_VERBOSITY=2 ./test/cali-basic
```
