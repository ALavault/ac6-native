// Every place a transform row is written from a vector ASSEMBLED ON THE STACK.
//
// Cycles 1132 and 1135 both classified 0x8229C0E0 as a "copy" because its
// vector arrives via lvx128 - from r1+0x50, a stack slot it had just filled
// from a record. Copying another object's row and assembling one out of data
// are opposite things and the earlier scans could not tell them apart. This
// separates them: a store whose vector was loaded from a stack-derived pointer
// is a value built here, and the instructions that filled that slot say from
// what.
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

public class Ac6StagedWrite extends GhidraScript {
    private static final Pattern ADDI = Pattern.compile("^addi (r\\d+),(r\\d+),(-?0x[0-9a-f]+)$");
    private static final Pattern LI = Pattern.compile("^li (r\\d+),(-?0x[0-9a-f]+)$");
    private static final Pattern LVX = Pattern.compile("^lvx128 (vr\\d+),(r\\d+),(r\\d+)$");
    private static final Pattern STVX = Pattern.compile("^stvx128 (vr\\d+),(r\\d+),(r\\d+)$");
    private static final Pattern WRITES = Pattern.compile("^\\S+\\s+(r\\d+),.*");

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        Set<Long> wanted = new HashSet<>();
        for (int i = 1; i < args.length; ++i) wanted.add(Long.decode(args[i]));
        PrintWriter out = new PrintWriter(args[0], "UTF-8");

        Set<String> stackPointer = new HashSet<>();   // registers formed from r1
        Map<String, Long> constant = new HashMap<>();
        Map<String, Long> bias = new HashMap<>();
        Set<String> stagedVector = new HashSet<>();   // vectors loaded from the stack
        int hits = 0, total = 0;

        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) {
            Instruction ins = it.next();
            String text = ins.toString();

            Matcher load = LVX.matcher(text);
            if (load.matches()) {
                String v = load.group(1), a = load.group(2), b = load.group(3);
                if (stackPointer.contains(a) || stackPointer.contains(b)) stagedVector.add(v);
                else stagedVector.remove(v);
                continue;
            }

            Matcher store = STVX.matcher(text);
            if (store.matches()) {
                String v = store.group(1), base = store.group(2), index = store.group(3);
                if (base.equals("r1") || stackPointer.contains(base)) continue;
                Long idx = index.equals("r0") ? Long.valueOf(0) : constant.get(index);
                if (idx == null) continue;
                long b = bias.containsKey(base) ? bias.get(base) : 0;
                if (!wanted.contains(b + idx)) continue;
                total++;
                if (!stagedVector.contains(v)) continue;
                Function f = getFunctionContaining(ins.getAddress());
                out.println(String.format("STAGED %s in %s | %s | effective=+0x%x",
                        ins.getAddress(),
                        f == null ? "none" : f.getName() + "@" + f.getEntryPoint(),
                        text, b + idx));
                hits++;
                continue;
            }

            Matcher addi = ADDI.matcher(text);
            if (addi.matches()) {
                String d = addi.group(1), s = addi.group(2);
                if (s.equals("r1") || stackPointer.contains(s)) stackPointer.add(d);
                else stackPointer.remove(d);
                if (s.equals("r1")) bias.remove(d);
                else bias.put(d, (bias.containsKey(s) ? bias.get(s) : 0) + Long.decode(addi.group(3)));
                constant.remove(d);
                continue;
            }

            Matcher li = LI.matcher(text);
            if (li.matches()) {
                constant.put(li.group(1), Long.decode(li.group(2)));
                bias.remove(li.group(1));
                stackPointer.remove(li.group(1));
                continue;
            }

            Matcher w = WRITES.matcher(text);
            if (w.matches()) {
                String d = w.group(1);
                constant.remove(d); bias.remove(d); stackPointer.remove(d);
            }
        }
        out.println("SUMMARY transform_row_writes=" + total + " assembled_on_stack=" + hits);
        out.close();
        println("WROTE staged=" + hits + " of " + total);
    }
}
