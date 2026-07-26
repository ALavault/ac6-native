// Read-only program provenance summary for AC6 project reconciliation.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.MemoryBlock;

public class DumpProgramIdentity extends GhidraScript {
    @Override
    protected void run() throws Exception {
        println("PROGRAM_NAME=" + currentProgram.getName());
        println("PROGRAM_DOMAIN=" + currentProgram.getDomainFile().getPathname());
        println("PROGRAM_EXECUTABLE=" + currentProgram.getExecutablePath());
        println("PROGRAM_IMAGE_BASE=" + currentProgram.getImageBase());
        println("PROGRAM_LANGUAGE=" + currentProgram.getLanguageID());
        println("PROGRAM_COMPILER=" + currentProgram.getCompilerSpec().getCompilerSpecID());
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            Address start = block.getStart();
            Address end = block.getEnd();
            println(String.format("BLOCK name=%s start=%s end=%s size=0x%x read=%s write=%s execute=%s initialized=%s",
                block.getName(), start, end, block.getSize(), block.isRead(),
                block.isWrite(), block.isExecute(), block.isInitialized()));
        }
    }
}
