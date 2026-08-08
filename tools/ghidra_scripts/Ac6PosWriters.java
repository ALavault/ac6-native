// Who writes an object's translation? The transform's fourth row is at +0x50, so
// a writer either stores a vector through a pointer formed as base+0x50, or
// stores three floats at 0x50/0x54/0x58 of the same base. Read-only. @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class Ac6PosWriters extends GhidraScript {
    private static final Pattern ADDI = Pattern.compile("^addi (r\\d+),(r\\d+),0x50$");
    private static final Pattern STVX = Pattern.compile("^stvx128 (vr\\d+),r0,(r\\d+)$");
    private static final Pattern STFS = Pattern.compile("^stfs (f\\d+),(0x5[048])\\((r\\d+)\\)$");

    @Override
    public void run() throws Exception {
        PrintWriter out = new PrintWriter(getScriptArgs()[0], "UTF-8");
        List<Instruction> all = new ArrayList<>();
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) all.add(it.next());

        int vector = 0, triple = 0;
        for (int i = 0; i < all.size(); ++i) {
            Matcher base = ADDI.matcher(all.get(i).toString());
            if (base.matches()) {
                String pointer = base.group(1);
                for (int k = i + 1; k < Math.min(i + 8, all.size()); ++k) {
                    Matcher store = STVX.matcher(all.get(k).toString());
                    if (store.matches() && store.group(2).equals(pointer)) {
                        report(out, "VECTOR", all.get(k), all.get(i).toString());
                        vector++;
                        break;
                    }
                }
                continue;
            }
            Matcher first = STFS.matcher(all.get(i).toString());
            if (!first.matches() || !first.group(2).equals("0x50")) continue;
            boolean has54 = false, has58 = false;
            for (int k = i + 1; k < Math.min(i + 10, all.size()); ++k) {
                Matcher m = STFS.matcher(all.get(k).toString());
                if (!m.matches() || !m.group(3).equals(first.group(3))) continue;
                if (m.group(2).equals("0x54")) has54 = true;
                if (m.group(2).equals("0x58")) has58 = true;
            }
            if (has54 && has58) { report(out, "TRIPLE", all.get(i), first.group(3)); triple++; }
        }
        out.println("SUMMARY vector=" + vector + " triple=" + triple);
        out.close();
        println("WROTE vector=" + vector + " triple=" + triple);
    }

    private void report(PrintWriter out, String kind, Instruction at, String detail) {
        Function f = getFunctionContaining(at.getAddress());
        out.println(String.format("%s %s in %s | %s | %s", kind, at.getAddress(),
                f == null ? "none" : f.getName() + "@" + f.getEntryPoint(), at, detail));
    }
}
