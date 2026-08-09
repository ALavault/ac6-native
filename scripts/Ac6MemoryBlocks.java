// The program's memory blocks, with their real bounds and permissions.
//
// Cycle 1348 tried to split .rdata into vtables using an OBSERVED .text bound and
// its own control failed: only 297 of 811 known vtable bases came back. A section
// boundary is a fact the program file carries; guessing it from where functions
// happen to live is not the same thing.
//
// Usage: -postScript Ac6MemoryBlocks.java
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.mem.MemoryBlock;

public class Ac6MemoryBlocks extends GhidraScript {
    @Override
    protected void run() throws Exception {
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            println(String.format("AC6_BLOCK %-12s %s..%s  %s%s%s  size=0x%x",
                block.getName(), block.getStart(), block.getEnd(),
                block.isRead() ? "r" : "-", block.isWrite() ? "w" : "-",
                block.isExecute() ? "x" : "-", block.getSize()));
        }
    }
}
