// Disassembles and decompiles sub_821D5F48 (the "must succeed"
// initialization step immediately before sub_821D6C20 in sub_821D7DE0's
// main-loop entry, r440) and the settings-reading helper functions
// sub_821D6C20 calls before its device-configuration block:
// sub_82234D40, sub_821CC008, sub_821CC508, sub_82222D80, sub_82222E08.
// Bounds taken from the qualified retail codegen's own function split
// points (next PPC_FUNC_IMPL after each), not .pdata -- none of these
// have a .pdata row either.
// Gated to the qualified retail XEX (6eefba42...), NOT the stale
// acc302c1... copy other scripts in this directory are gated to.
// Run WITHOUT -readOnly: creates these functions in the current project
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

public class DecompileD5F48SettingsHelpers extends GhidraScript {

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
            println("AC6_D5F48 " + label + " FAILED_TO_CREATE_FUNCTION");
            return;
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
        println("=== " + label + " " + startAddr + " ===");
        if (!result.decompileCompleted()) {
            println("AC6_D5F48 " + label + " DECOMPILE_FAILED " + result.getErrorMessage());
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
        disassembleAndDecompile(0x821D5F48L, 0x821D6C1CL, "sub_821D5F48");
        disassembleAndDecompile(0x82234D40L, 0x82234D74L, "sub_82234D40");
        disassembleAndDecompile(0x821CC008L, 0x821CC284L, "sub_821CC008");
        disassembleAndDecompile(0x821CC508L, 0x821CD07CL, "sub_821CC508");
        disassembleAndDecompile(0x82222D80L, 0x82222E08L, "sub_82222D80");
        disassembleAndDecompile(0x82222E08L, 0x82222E68L, "sub_82222E08");
    }
}
