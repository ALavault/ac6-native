// Every vpermwi128 in the image: address, raw instruction word, and the
// immediate THIS module decoded. Cycle 1325 measured Ghidra and XenonRecomp
// disagreeing on that immediate at three sites; this is the population.
//
// Usage: -postScript Ac6VpermwiSites.java START END OUT_TSV
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;
import java.io.PrintWriter;

public class Ac6VpermwiSites extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        Address start = toAddr(Long.decode(args[0]) & 0xffffffffL);
        Address end = toAddr(Long.decode(args[1]) & 0xffffffffL);
        int found = 0;
        try (PrintWriter out = new PrintWriter(args[2])) {
            out.println("# vpermwi128 sites: address, instruction word, the immediate");
            out.println("# this SLEIGH module decodes, and the destination/source it names.");
            out.println("# Produced by scripts/Ac6VpermwiSites.java. Read-only, no oracle.");
            out.println("# columns: address word ghidra_immediate vd vb");
            for (Instruction instruction : currentProgram.getListing()
                    .getInstructions(start, true)) {
                if (instruction.getAddress().compareTo(end) >= 0) {
                    break;
                }
                if (!"vpermwi128".equals(instruction.getMnemonicString())) {
                    continue;
                }
                byte[] bytes = instruction.getBytes();
                long word = 0;
                for (byte value : bytes) {
                    word = (word << 8) | (value & 0xFF);
                }
                Object[] destination = instruction.getOpObjects(0);
                Object[] source = instruction.getOpObjects(1);
                Object[] immediate = instruction.getOpObjects(2);
                out.printf("%s\t0x%08x\t0x%02x\t%s\t%s%n",
                    instruction.getAddress(), word,
                    ((Scalar) immediate[0]).getUnsignedValue(),
                    destination[0], source[0]);
                found++;
            }
        }
        println("AC6_VPERMWI sites=" + found);
    }
}
