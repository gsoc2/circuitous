#!/bin/sh

sh scripts/from-mttn-trace.sh input_mttn_traces/nop.trace.txt; (( out = out || $? ))

exit $out
