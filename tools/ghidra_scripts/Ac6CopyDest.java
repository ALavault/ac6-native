// memcpy calls whose DESTINATION is a transform row, whatever the size.
//
// Cycle 1138 filtered copies by size and found none writing a unit transform.
// A variable-size copy escapes that. This filters by destination instead: any
// bl to memcpy (0x82382F70) whose r3 was formed as addi r3,rX,imm with imm one
// of the transform offsets and rX not the stack.
//
// Read-only. @category AC6
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

import java.io.PrintWriter;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class Ac6CopyDest extends GhidraScript {
    private static final Pattern ADDI = Pattern.compile("^addi (r\\d+),(r\\d+),(0x[0-9a-f]+)$");
    private static final Pattern BL = Pattern.compile("^bl (0x[0-9a-f]+)$");
    private static final Pattern WRITES = Pattern.compile("^\\S+\\s+(r\\d+),.*");

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        Set<Long> rows = new HashSet<>();
        for (long v : new long[]{0x20, 0x50, 0x60, 0x70, 0xa0}) rows.add(v);
        PrintWriter out = new PrintWriter(args[0], "UTF-8");
        Map<String, Long> off = new HashMap<>();
        Set<String> stack = new HashSet<>();
        int hits = 0, calls = 0;

        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) {
            Instruction ins = it.next();
            String text = ins.toString();
            Matcher bl = BL.matcher(text);
            if (bl.matches()) {
                if (bl.group(1).equals("0x82382f70")) {
                    calls++;
                    Long d = off.get("r3");
                    if (d != null && rows.contains(d) && !stack.contains("r3")) {
                        Function f = getFunctionContaining(ins.getAddress());
                        out.println(String.format("DEST %s in %s | destination = base+0x%x",
                                ins.getAddress(),
                                f == null ? "none" : f.getName() + "@" + f.getEntryPoint(), d));
                        hits++;
                    }
                }
                off.clear(); stack.clear();
                continue;
            }
            Matcher a = ADDI.matcher(text);
            if (a.matches()) {
                String d = a.group(1), s = a.group(2);
                off.put(d, Long.decode(a.group(3)));
                if (s.equals("r1") || stack.contains(s)) stack.add(d); else stack.remove(d);
                continue;
            }
            Matcher w = WRITES.matcher(text);
            if (w.matches()) { off.remove(w.group(1)); stack.remove(w.group(1)); }
        }
        out.println("SUMMARY memcpy_calls=" + calls + " into_a_transform_row=" + hits);
        out.close();
        println("WROTE calls=" + calls + " hits=" + hits);
    }
}
