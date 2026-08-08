// The constructor writes the transform with indexed vector stores - li rB,0x40 /
// stvx128 vrN,rA,rB - not with a displacement, so a scalar scan for "0x50("
// cannot see a placer that uses the same idiom. This looks for the idiom.
// Read-only. @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class Ac6VecStore extends GhidraScript {
    private static final Pattern LI = Pattern.compile("^li (r\\d+),(0x[0-9a-f]+)$");
    private static final Pattern STVX = Pattern.compile("^stvx128 (vr\\d+),(r\\d+),(r\\d+)$");

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        String wanted = args[1];
        PrintWriter out = new PrintWriter(args[0], "UTF-8");
        List<Instruction> all = new ArrayList<>();
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) all.add(it.next());

        Map<String, String> constants = new HashMap<>();
        int hits = 0;
        for (Instruction ins : all) {
            String text = ins.toString();
            Matcher li = LI.matcher(text);
            if (li.matches()) { constants.put(li.group(1), li.group(2)); continue; }
            Matcher store = STVX.matcher(text);
            if (!store.matches()) {
                // Any other write to the index register invalidates what we knew.
                for (String r : new ArrayList<>(constants.keySet())) {
                    if (text.matches("^\\S+ " + r + ",.*")) constants.remove(r);
                }
                continue;
            }
            String index = store.group(3);
            if (!wanted.equals(constants.get(index))) continue;
            Function f = getFunctionContaining(ins.getAddress());
            out.println(String.format("STORE %s in %s | %s | base=%s", ins.getAddress(),
                    f == null ? "none" : f.getName() + "@" + f.getEntryPoint(), text,
                    store.group(2)));
            hits++;
        }
        out.println("SUMMARY offset=" + wanted + " hits=" + hits);
        out.close();
        println("WROTE hits=" + hits);
    }
}
