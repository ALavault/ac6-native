// Disassembles and decompiles sub_823CFE08 -- the function containing the
// real guest dispatch target (0x823D035C) that r447 live-resolved for
// sub_821F4E70's kernel/device-object indirect call. Confirms whether the
// error 0x13D observed by r446 comes from a genuine data/state condition
// in this offline environment or a codegen defect local to this function.
// Bounds taken from the qualified retail codegen's own function split
// points (next PPC_FUNC_IMPL after it); no .pdata row for this address.
// Gated to the qualified retail XEX (6eefba42...), NOT the stale
// acc302c1... copy other scripts in this directory are gated to.
// Run WITHOUT -readOnly: creates this function in the current project so
// the decompiler has something to work from. Local, reversible (the
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

public class DecompileD03D5CDispatchTarget extends GhidraScript {

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
            println("AC6_D03D5C " + label + " FAILED_TO_CREATE_FUNCTION");
            return;
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
        println("=== " + label + " " + startAddr + " ===");
        if (!result.decompileCompleted()) {
            println("AC6_D03D5C " + label + " DECOMPILE_FAILED " + result.getErrorMessage());
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
        disassembleAndDecompile(0x823D035CL, 0x823D0E00L, "target_823D035C");
    }
}
