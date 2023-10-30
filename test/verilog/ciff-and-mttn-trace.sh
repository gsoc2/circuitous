#!/bin/zsh

# Nothing can produce an error
set -e -x

ciff=$1
mttn_trace=$2

circuit_root=`basename $ciff ".ciff"`
circuit_v="$circuit_root.v"
circuit_c="$circuit_root.circir"

# We are caching as this can be quite expensive time wise for
# prototyping
if [[ -f $circuit_v && -f  $circuit_c ]]; then
    echo "Found cached circuit files: $circuit_v $circuit_c"
else
    $lift --os macos \
          --arch x86 \
          --logtostderr \
          --quiet \
          --lift-with disjunctions \
          --ciff-in $ciff \
          --verilog-out $circuit_v \
          --ir-out $circuit_c
fi

mttn_root=`basename $mttn_trace ".mttn"`
trace_out="$mttn_root.$circuit_root.circuitous_trace"
if [ ! -f $trace_out ]; then
    echo "Converting traces: $mttn -> $trace_out"
    $run --ir-in $circuit_c \
         --convert-trace mttn \
         --traces $mttn_trace \
         --quiet \
         --output $trace_out
else
    echo "Found cached trace $trace_out"
fi

circuitous-test-harness.sh $circuit_v $trace_out
