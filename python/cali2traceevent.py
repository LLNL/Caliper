#!/usr/bin/env python3

# Convert a .cali trace to Google TraceEvent JSON

import json
import sys

import caliperreader


def _get_first_from_list(rec, attribute_list, fallback=0):
    for attr in attribute_list:
        if attr in rec:
            return rec[attr]
    return fallback

def _get_timestamp(rec):
    """Get timestamp from rec and convert to milliseconds
    """

    TS_ATTRIBUTES = {
        "cupti.timestamp"      : 1e3,
        "cupti.activity.start" : 1e3,
        "cupti.activity.end"   : 1e3,
        "time.offset"          : 1.0
    }

    for attr,factor in TS_ATTRIBUTES.items():
        if attr in rec:
            return float(rec[attr]) * factor

    return None

class CaliTraceEventConverter:
    PID_ATTRIBUTES = [
        'mpi.rank'
    ]

    TID_ATTRIBUTES = [
        'omp.thread.id',
        'pthread.id'
    ]

    TS_ATTRIBUTES = [
        'cupti.timestamp',
        'cupti.activity.start',
        'cupti.activity.end',
        'time.offset'
    ]

    def __init__(self):
        self.records = []
        self.reader  = caliperreader.CaliperStreamReader()

        self.skipped = 0
        self.written = 0

    def read(self, filename_or_stream):
        self.reader.read(filename_or_stream, self._process_record)

    def write(self, output):
        json.dump(self.records, output)
        self.written += len(self.records)

    def _process_record(self, rec):
        pid  = int(_get_first_from_list(rec, self.PID_ATTRIBUTES))
        tid  = int(_get_first_from_list(rec, self.TID_ATTRIBUTES))

        trec = {}
        keys = list(rec.keys())

        for key in keys:
            if key.startswith("event.end#"):
                self._process_event_end(rec, key, trec)
                break

        trec.update(pid=pid,tid=tid)

        if "name" in trec:
            self.records.append(trec)
        else:
            self.skipped += 1

    def _process_event_end(self, rec, key, trec):
        attr = key[len("event.end#"):]

        if not self.reader.attribute(attr).is_nested():
            return

        tst = _get_timestamp(rec)
        dur = rec.get("time.inclusive.duration", None)

        if tst is None or dur is None:
            return

        dur = float(dur)*1e6
        tst = tst-dur

        trec.update(ph="X", name=rec[key], cat=attr, ts=tst, dur=dur)


def main():
    args = sys.argv[1:]

    if len(args) < 1:
        sys.exit("Usage: cali2traceevent.py 1.cali 2.cali ... [output.json]")

    output = sys.stdout

    if len(args) > 1 and args[-1].endswith(".json"):
        output = open(args.pop(), "w")

    converter = CaliTraceEventConverter()

    for f in args:
        with open(f) as input:
            converter.read(f)

    converter.write(output)

    if output is not sys.stdout:
        output.close()

    wrt = converter.written
    skp = converter.skipped

    print("{} records written, {} skipped.".format(wrt, skp), file=sys.stderr)


if __name__ == "__main__":
    main()
