#!/bin/zsh

# Nothing can produce an error
set -e -x

mttn_trace=$1

circuit_root=`basename $mttn_trace ".mttn"`
circuit_v="$circuit_root.v"
circuit_c="$circuit_root.circir"

trace_out="$circuit_root.$circuit_root.circuitous_trace"
if [[ -f $trace_out && -f $circuit_c && -f $circuit_v ]]; then
    echo "Found cached trace $trace_out"
else
    echo "Converting traces and producing circuit: $mttn -> $trace_out"
    $run --convert-trace mttn \
         --traces $mttn_trace \
         --quiet \
         --construct-circuit \
         --verilog-out $circuit_v \
         --ir-out $circuit_c \
         --output $trace_out
fi

circuitous-test-harness.sh $circuit_v $trace_out
