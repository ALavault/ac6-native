// Calls whose third argument is a transform-sized constant.
//
// A position written by memcpy(obj+0x50, src, 12) is invisible to every store
// sweep this campaign has run. This finds the candidates: any bl whose r5 was
// last set by li r5,N for N in {12, 16, 48, 64}, grouped by callee, with the
// caller recorded. The callee that dominates such calls is the copy primitive.
//
// Read-only. @category AC6
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

import java.io.PrintWriter;
import java.util.HashMap;
import java.util.Map;
import java.util.TreeMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class Ac6CopyCalls extends GhidraScript {
    private static final Pattern LI = Pattern.compile("^li (r\\d+),(0x[0-9a-f]+)$");
    private static final Pattern BL = Pattern.compile("^bl (0x[0-9a-f]+)$");
    private static final Pattern WRITES = Pattern.compile("^\\S+\\s+(r\\d+),.*");

    @Override
    public void run() throws Exception {
        PrintWriter out = new PrintWriter(getScriptArgs()[0], "UTF-8");
        Map<String, Long> constant = new HashMap<>();
        Map<String, Integer> byCallee = new TreeMap<>();
        int hits = 0;
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) {
            Instruction ins = it.next();
            String text = ins.toString();
            Matcher bl = BL.matcher(text);
            if (bl.matches()) {
                Long size = constant.get("r5");
                if (size != null && (size == 12 || size == 16 || size == 48 || size == 64)) {
                    Function f = getFunctionContaining(ins.getAddress());
                    out.println(String.format("CALL %s -> %s size=%d in %s",
                            ins.getAddress(), bl.group(1), size,
                            f == null ? "none" : f.getName() + "@" + f.getEntryPoint()));
                    byCallee.merge(bl.group(1) + " size=" + size, 1, Integer::sum);
                    hits++;
                }
                constant.clear();   // a call clobbers the volatile registers
                continue;
            }
            Matcher li = LI.matcher(text);
            if (li.matches()) { constant.put(li.group(1), Long.decode(li.group(2))); continue; }
            Matcher w = WRITES.matcher(text);
            if (w.matches()) constant.remove(w.group(1));
        }
        out.println("-- by callee --");
        byCallee.entrySet().stream()
                .sorted((a, b) -> b.getValue() - a.getValue())
                .limit(20)
                .forEach(e -> out.println(String.format("  %-24s %d", e.getKey(), e.getValue())));
        out.println("SUMMARY calls=" + hits);
        out.close();
        println("WROTE hits=" + hits);
    }
}
