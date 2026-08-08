// Find where the ObjBin record an object files at +0x180 is dereferenced to its
// data block, and what is read out of that block.
//
// The pattern: some register takes object+0x180, then within a short window the
// same register is dereferenced at +0x00 - the ObjBin record's data pointer -
// and something is read from the result. Read-only. @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class Ac6ChainScan extends GhidraScript {
    private static final Pattern LOAD_AT =
            Pattern.compile("^lwz (r\\d+),(0x[0-9a-f]+)\\((r\\d+)\\)$");

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        String anchorOffset = args[1];              // e.g. 0x180
        int window = Integer.parseInt(args[2]);     // instructions to look ahead
        PrintWriter out = new PrintWriter(args[0], "UTF-8");

        List<Instruction> all = new ArrayList<>();
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) all.add(it.next());

        int hits = 0;
        for (int i = 0; i < all.size(); ++i) {
            Matcher anchor = LOAD_AT.matcher(all.get(i).toString());
            if (!anchor.matches() || !anchor.group(2).equals(anchorOffset)) continue;
            String record = anchor.group(1);
            for (int k = i + 1; k < Math.min(i + 1 + window, all.size()); ++k) {
                Instruction later = all.get(k);
                Matcher deref = LOAD_AT.matcher(later.toString());
                if (!deref.matches() || !deref.group(3).equals(record)
                        || !deref.group(2).equals("0x0")) {
                    // The record register being overwritten ends the window.
                    if (deref.matches() && deref.group(1).equals(record)) break;
                    continue;
                }
                String data = deref.group(1);
                StringBuilder reads = new StringBuilder();
                for (int j = k + 1; j < Math.min(k + 1 + window, all.size()); ++j) {
                    String text = all.get(j).toString();
                    if (text.matches("^(lfs|lvlx|lvx128|lbz|lhz|lwz) .*\\(" + data + "\\)$")
                            || text.contains("," + data + ")")) {
                        reads.append(' ').append(text);
                    }
                    if (text.matches("^(lwz|li|addi|or) " + data + ",.*")) break;
                }
                Function f = getFunctionContaining(all.get(i).getAddress());
                out.println(String.format("CHAIN %s in %s |%s", all.get(i).getAddress(),
                        f == null ? "none" : f.getName() + "@" + f.getEntryPoint(),
                        reads.toString()));
                hits++;
                break;
            }
        }
        out.println("CHAIN_SUMMARY hits=" + hits);
        out.close();
        println("WROTE hits=" + hits);
    }
}
