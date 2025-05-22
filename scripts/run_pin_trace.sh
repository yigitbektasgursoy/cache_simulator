#!/bin/bash
# Script to run a program with PIN memory tracing and save to a trace file

# Default settings
PIN_ROOT="/home/yigit/bin/pin-external-3.31-98869-gfa6f126a8-gcc-linux"
PIN_TOOL="$PIN_ROOT/source/tools/CacheTracer/obj-intel64/roi_tracer.so"
OUTPUT_FILE="cache_trace.out"

# Parse command line arguments
show_usage() {
    echo "Usage: $0 [options] -- program [program_args]"
    echo "Options:"
    echo "  -p, --pin-root PATH     PIN installation directory (default: $PIN_ROOT)"
    echo "  -t, --tool PATH         Full path to PIN tool (default: $PIN_TOOL)"
    echo "  -o, --output FILE       Output trace file (default: $OUTPUT_FILE)"
    echo "  -h, --help              Show this help message"
    exit 1
}

# Parse options
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--pin-root)
            PIN_ROOT="$2"
            shift 2
            ;;
        -t|--tool)
            PIN_TOOL="$2"
            shift 2
            ;;
        -o|--output)
            OUTPUT_FILE="$2"
            shift 2
            ;;
        -h|--help)
            show_usage
            ;;
        --)
            shift
            break
            ;;
        *)
            echo "Unknown option: $1"
            show_usage
            ;;
    esac
done

# Check if program is specified
if [ $# -eq 0 ]; then
    echo "Error: No program specified."
    show_usage
fi

# Check that PIN_ROOT exists
if [ ! -d "$PIN_ROOT" ]; then
    echo "Error: PIN directory not found: $PIN_ROOT"
    exit 1
fi

# Check that tool exists
if [ ! -f "$PIN_TOOL" ]; then
    echo "Error: PIN tool not found: $PIN_TOOL"
    echo "Make sure you build the PIN tool first."
    exit 1
fi

echo "Running program with PIN instrumentation..."
echo "Output trace file: $OUTPUT_FILE"

"$PIN_ROOT/pin" -t "$PIN_TOOL" -o "$OUTPUT_FILE" -- "$@"

echo "Trace file generated: $OUTPUT_FILE"
echo "You can use this trace file with the cache simulator."
