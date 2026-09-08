// Disassembles and decompiles sub_821D4988 -- called immediately before
// the confirmed-taken -1 return inside sub_821CC508 (r444), with a
// constant argument (0x8275a414) whose own return value is never
// checked, matching a "log then fail" shape.
// Bounds taken from the qualified retail codegen's own function split
// points (next PPC_FUNC_IMPL after it); no .pdata row for this address.
// Gated to the qualified retail XEX (6eefba42...), NOT the stale
// acc302c1... copy other scripts in this directory are gated to.
// Run WITHOUT -readOnly: creates this function in the current project
// so the decompiler has something to work from. Local, reversible (the
// project directory is gitignored), read-only in effect for the rest of
// the binary -- no other function is touched.
// @category AC6

import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;

public class DecompileD4988Logger extends GhidraScript {

    private static final String QUALIFIED_XEX_SHA256 =
        "6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc";

    private void disassembleAndDecompile(long start, long end, String label)
            throws Exception {
        Address startAddr = toAddr(start);
        Disassembler disassembler =
            Disassembler.getDisassembler(currentProgram, monitor, null);
        AddressSet seeds = new AddressSet();
        for (long cursor = start; cursor < end; cursor += 4) {
            Address address = toAddr(cursor);
            if (getInstructionAt(address) == null) {
                seeds.add(address);
            }
        }
        for (Address seed : seeds.getAddresses(true)) {
            if (getInstructionAt(seed) == null) {
                disassembler.disassemble(seed, null);
            }
        }
        CreateFunctionCmd createCmd = new CreateFunctionCmd(startAddr);
        createCmd.applyTo(currentProgram, monitor);
        Function function =
            currentProgram.getFunctionManager().getFunctionAt(startAddr);
        if (function == null) {
            println("AC6_D4988 " + label + " FAILED_TO_CREATE_FUNCTION");
            return;
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
        println("=== " + label + " " + startAddr + " ===");
        if (!result.decompileCompleted()) {
            println("AC6_D4988 " + label + " DECOMPILE_FAILED " + result.getErrorMessage());
        } else {
            println(result.getDecompiledFunction().getC());
        }
        decompiler.dispose();
    }

    @Override
    protected void run() throws Exception {
        String sha = currentProgram.getExecutableSHA256();
        if (!QUALIFIED_XEX_SHA256.equalsIgnoreCase(sha)) {
            throw new IllegalStateException("unexpected XEX SHA-256: " + sha
                + " (expected " + QUALIFIED_XEX_SHA256 + ")");
        }
        disassembleAndDecompile(0x821D4988L, 0x821D4A28L, "sub_821D4988");
    }
}
