// A spawn must set both copies of the transform: the live translation at
// object+0x50 and the previous-frame mirror at object+0xA0 that the constructor
// makes at 0x820A7810-0x820A7818. Writing only the first would leave the object
// interpolating from the origin. That pair is far narrower than +0x50 alone.
// Read-only. @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class Ac6SpawnScan extends GhidraScript {
    // Stores whose displacement is written literally, plus pointers formed by addi.
    private static final Pattern STORE =
            Pattern.compile("^(stfs|stw|std) \\S+,(0x[0-9a-f]+)\\((r\\d+)\\)$");
    private static final Pattern ADDI = Pattern.compile("^addi (r\\d+),(r\\d+),(0x[0-9a-f]+)$");

    @Override
    public void run() throws Exception {
        PrintWriter out = new PrintWriter(getScriptArgs()[0], "UTF-8");
        Map<String, Set<String>> byFunction = new HashMap<>();
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) {
            Instruction ins = it.next();
            String text = ins.toString();
            String offset = null;
            Matcher store = STORE.matcher(text);
            if (store.matches()) offset = store.group(2);
            else {
                Matcher addi = ADDI.matcher(text);
                if (addi.matches()) offset = addi.group(3);
            }
            if (offset == null) continue;
            if (!offset.equals("0x50") && !offset.equals("0xa0")) continue;
            Function f = getFunctionContaining(ins.getAddress());
            String key = f == null ? "none@" + ins.getAddress() : f.getName() + "@" + f.getEntryPoint();
            byFunction.computeIfAbsent(key, k -> new HashSet<>()).add(offset);
        }
        int both = 0;
        for (Map.Entry<String, Set<String>> entry : byFunction.entrySet()) {
            if (entry.getValue().size() < 2) continue;
            out.println("BOTH " + entry.getKey());
            both++;
        }
        out.println("SUMMARY functions_touching_either=" + byFunction.size() + " both=" + both);
        out.close();
        println("WROTE both=" + both);
    }
}
