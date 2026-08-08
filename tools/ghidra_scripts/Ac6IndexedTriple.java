// The indexed-store blind spot, attacked without value propagation.
//
// Cycle 1133 counted 659 indexed stores whose index register is not a
// followable constant, and cycles 1133 and 1135 both declined to add value
// propagation for them. This does not need it: writing a three-float position
// looks like three stfsx to the SAME base register, with three different float
// sources, close together - whatever the indices evaluate to. That signature is
// resolvable with no propagation at all.
//
// Read-only. @category AC6
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class Ac6IndexedTriple extends GhidraScript {
    private static final Pattern STFSX = Pattern.compile("^stfsx (f\\d+),(r\\d+),(r\\d+)$");

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        int window = Integer.parseInt(args[1]);
        PrintWriter out = new PrintWriter(args[0], "UTF-8");
        List<Instruction> all = new ArrayList<>();
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) all.add(it.next());

        int hits = 0;
        for (int i = 0; i < all.size(); ++i) {
            Matcher first = STFSX.matcher(all.get(i).toString());
            if (!first.matches()) continue;
            String base = first.group(2);
            if (base.equals("r1")) continue;
            Set<String> sources = new HashSet<>();
            Set<String> indices = new HashSet<>();
            sources.add(first.group(1));
            indices.add(first.group(3));
            int count = 1;
            for (int k = i + 1; k < Math.min(i + 1 + window, all.size()); ++k) {
                Matcher m = STFSX.matcher(all.get(k).toString());
                if (!m.matches() || !m.group(2).equals(base)) continue;
                sources.add(m.group(1));
                indices.add(m.group(3));
                count++;
            }
            // Three stores, three distinct destinations, at least two distinct
            // values: a vector, not a memset and not one field written thrice.
            if (count < 3 || indices.size() < 3 || sources.size() < 2) continue;
            Function f = getFunctionContaining(all.get(i).getAddress());
            out.println(String.format("TRIPLE %s in %s | base=%s stores=%d sources=%d | %s",
                    all.get(i).getAddress(),
                    f == null ? "none" : f.getName() + "@" + f.getEntryPoint(),
                    base, count, sources.size(), all.get(i)));
            hits++;
            i += count - 1;
        }
        out.println("SUMMARY indexed_vector_writes=" + hits);
        out.close();
        println("WROTE hits=" + hits);
    }
}
