#include "pin.H"
#include <iostream>
#include <fstream>
#include <string> // For IMG_Name

std::ofstream TraceFile;
bool inROI = false;        // Global flag to indicate if we are currently in the ROI
UINT64 accessesInCurrentROI = 0; // Counter for accesses within the current ROI segment
UINT64 totalAccessesRecorded = 0; // Counter for total accesses across all ROI segments

// Knob for output file name
KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "memtrace.out", "specify output file name for memory trace");

// Analysis routine called by Pin when the application's begin_roi() is executed
VOID AppBeginROIHandler() {
    if (!inROI) {
        inROI = true;
        accessesInCurrentROI = 0; // Reset for the new ROI segment
        std::cerr << "PINTOOL: ==> Entering Region of Interest." << std::endl;
    }
}

// Analysis routine called by Pin when the application's end_roi() is executed
VOID AppEndROIHandler() {
    if (inROI) {
        inROI = false;
        std::cerr << "PINTOOL: <== Exiting Region of Interest. Accesses in this segment: "
                  << accessesInCurrentROI << std::endl;
        totalAccessesRecorded += accessesInCurrentROI;
    }
}

// This function is called before every memory access
VOID RecordMemAccess(VOID *addr, BOOL isWrite) {
    // Only record if we're in the Region of Interest
    if (!inROI) return;

    TraceFile << (isWrite ? "W " : "R ") << std::hex << addr << std::endl;
    // Alternative format matching some common trace tools:
    // TraceFile << (isWrite ? 'W' : 'R') << " 0x" << std::hex << (ADDRINT)addr << std::endl;

    accessesInCurrentROI++;
}

// Instrument memory accesses
// This function is called for every instruction.
VOID Instruction(INS ins, VOID *v) {
    // We only instrument memory operations if the inROI flag is true.
    // The flag is controlled by AppBeginROIHandler and AppEndROIHandler.

    // Instrument memory reads
    if (INS_IsMemoryRead(ins)) {
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, (AFUNPTR)RecordMemAccess,
            IARG_MEMORYREAD_EA,
            IARG_BOOL, FALSE, // isWrite = false
            IARG_PREDICATE,   // Only call if the instruction predicated is true
            IARG_END);
    }

    // Instrument memory writes
    if (INS_IsMemoryWrite(ins)) {
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, (AFUNPTR)RecordMemAccess,
            IARG_MEMORYWRITE_EA,
            IARG_BOOL, TRUE, // isWrite = true
            IARG_PREDICATE,  // Only call if the instruction predicated is true
            IARG_END);
    }
    
    // If an instruction is both read and write (e.g. xadd), it needs two separate calls
    // The above handles typical RISC-like instructions well.
    // For more complex CISC instructions that might have multiple memory operands:
    // UINT32 memOperands = INS_MemoryOperandCount(ins);
    // for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
    //     if (INS_MemoryOperandIsRead(ins, memOp)) {
    //         INS_InsertPredicatedCall(
    //             ins, IPOINT_BEFORE, (AFUNPTR)RecordMemAccess,
    //             IARG_MEMORYOP_EA, memOp,
    //             IARG_BOOL, FALSE,
    //             IARG_PREDICATE,
    //             IARG_END);
    //     }
    //     if (INS_MemoryOperandIsWritten(ins, memOp)) {
    //         INS_InsertPredicatedCall(
    //             ins, IPOINT_BEFORE, (AFUNPTR)RecordMemAccess,
    //             IARG_MEMORYOP_EA, memOp,
    //             IARG_BOOL, TRUE,
    //             IARG_PREDICATE,
    //             IARG_END);
    //     }
    // }
}


// Image load callback - find and instrument ROI marker functions
VOID ImageLoad(IMG img, VOID *v) {
    std::cerr << "PINTOOL: Scanning image " << IMG_Name(img) << " for ROI markers." << std::endl;

    // Look for the application's begin_roi function
    // You might need to adjust the name if C++ name mangling occurs or if it's a C function
    // For C functions, the name is usually preserved as is or with a leading underscore.
    RTN beginRtn = RTN_FindByName(img, "begin_roi");
    if (!RTN_Valid(beginRtn)) { // Try with leading underscore for some C compilers/linkers
        beginRtn = RTN_FindByName(img, "_begin_roi");
    }

    if (RTN_Valid(beginRtn)) {
        RTN_Open(beginRtn);
        // Instrument right at the beginning of the function
        RTN_InsertCall(beginRtn, IPOINT_BEFORE, (AFUNPTR)AppBeginROIHandler, IARG_END);
        RTN_Close(beginRtn);
        std::cerr << "PINTOOL: Instrumented " << RTN_Name(beginRtn) << " in " << IMG_Name(img) << std::endl;
    } else {
        // This is not necessarily an error if the main executable doesn't have them,
        // but a library it loads might. So just a note.
        // std::cerr << "PINTOOL: NOTE - begin_roi function not found in " << IMG_Name(img) << std::endl;
    }

    // Look for the application's end_roi function
    RTN endRtn = RTN_FindByName(img, "end_roi");
    if (!RTN_Valid(endRtn)) { // Try with leading underscore
        endRtn = RTN_FindByName(img, "_end_roi");
    }

    if (RTN_Valid(endRtn)) {
        RTN_Open(endRtn);
        // Instrument right at the beginning of the function
        RTN_InsertCall(endRtn, IPOINT_BEFORE, (AFUNPTR)AppEndROIHandler, IARG_END);
        RTN_Close(endRtn);
        std::cerr << "PINTOOL: Instrumented " << RTN_Name(endRtn) << " in " << IMG_Name(img) << std::endl;
    } else {
        // std::cerr << "PINTOOL: NOTE - end_roi function not found in " << IMG_Name(img) << std::endl;
    }
}

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v) {
    if (TraceFile.is_open()) {
        TraceFile.close();
    }
    std::cerr << "PINTOOL: Application finished." << std::endl;
    std::cerr << "PINTOOL: Total memory accesses recorded across all ROIs: " << totalAccessesRecorded << std::endl;
}


INT32 Usage() {
    std::cerr << PIN_ToolName("roi_tracer") << ":" << std::endl;
    std::cerr << "  Tool to trace memory accesses within specified Regions of Interest (ROI)." << std::endl;
    std::cerr << "  The application being traced should call functions named 'begin_roi()' and 'end_roi()'" << std::endl;
    std::cerr << "  to mark these regions." << std::endl;
    std::cerr << std::endl << KNOB_BASE::StringKnobSummary() << std::endl;
    return -1;
}

int main(int argc, char *argv[]) {
    // Initialize Pin
    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    // Open the trace file
    TraceFile.open(KnobOutputFile.Value().c_str());
    if (!TraceFile.is_open()) {
        std::cerr << "ERROR: Unable to open output trace file '" << KnobOutputFile.Value() << "'" << std::endl;
        return 1;
    }
    std::cerr << "PINTOOL: Trace file opened: " << KnobOutputFile.Value() << std::endl;

    // Register ImageLoad to find ROI functions when images (executables/libraries) are loaded
    IMG_AddInstrumentFunction(ImageLoad, 0);

    // Register Instruction to instrument memory accesses (will only record if inROI is true)
    INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0; // Should not reach here
}
