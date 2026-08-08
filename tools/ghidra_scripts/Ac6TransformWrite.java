// Who writes an object's translation row, whatever idiom they use?
//
// Cycle 1128 stopped here: the scans looked for the literal displacement 0x50,
// but the unit constructor writes its transform with indexed vector stores -
// `addi rA,rObj,0x20 ; li rB,0x30 ; stvx128 vrN,rA,rB` reaches +0x50 without
// the number 0x50 appearing anywhere. This resolves the effective offset:
// it tracks how a base register was formed (`addi rA,rSrc,imm`) and what
// constant an index register holds (`li rB,imm`), and reports every vector
// store whose base bias plus index equals the wanted offset.
//
// Read-only. @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

import java.io.PrintWriter;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class Ac6TransformWrite extends GhidraScript {
    private static final Pattern ADDI = Pattern.compile("^addi (r\\d+),(r\\d+),(-?0x[0-9a-f]+)$");
    private static final Pattern LI = Pattern.compile("^li (r\\d+),(-?0x[0-9a-f]+)$");
    private static final Pattern STVX = Pattern.compile("^stvx128 (vr\\d+),(r\\d+),(r\\d+)$");
    private static final Pattern WRITES = Pattern.compile("^\\S+\\s+(r\\d+),.*");

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        long wanted = Long.decode(args[1]);
        PrintWriter out = new PrintWriter(args[0], "UTF-8");

        // base register -> {source register, bias}; index register -> constant
        Map<String, String> baseSource = new HashMap<>();
        Map<String, Long> baseBias = new HashMap<>();
        Map<String, Long> constant = new HashMap<>();
        int hits = 0;

        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) {
            Instruction ins = it.next();
            String text = ins.toString();

            Matcher store = STVX.matcher(text);
            if (store.matches()) {
                String base = store.group(2), index = store.group(3);
                Long idx = index.equals("r0") ? Long.valueOf(0) : constant.get(index);
                if (idx == null) continue;
                long bias = baseBias.containsKey(base) ? baseBias.get(base) : 0;
                if (bias + idx != wanted) continue;
                Function f = getFunctionContaining(ins.getAddress());
                out.println(String.format("WRITE %s in %s | %s | base=%s bias=0x%x index=0x%x src=%s",
                        ins.getAddress(), f == null ? "none" : f.getName() + "@" + f.getEntryPoint(),
                        text, base, bias, idx,
                        baseSource.containsKey(base) ? baseSource.get(base) : base));
                hits++;
                continue;
            }

            Matcher addi = ADDI.matcher(text);
            if (addi.matches()) {
                String dest = addi.group(1), src = addi.group(2);
                long imm = Long.decode(addi.group(3));
                // A pointer built on the stack is a local, never an object.
                if (src.equals("r1")) { baseBias.remove(dest); baseSource.remove(dest); }
                else {
                    long inherited = baseBias.containsKey(src) ? baseBias.get(src) : 0;
                    baseBias.put(dest, inherited + imm);
                    baseSource.put(dest, baseSource.containsKey(src) ? baseSource.get(src) : src);
                }
                constant.remove(dest);
                continue;
            }

            Matcher li = LI.matcher(text);
            if (li.matches()) {
                constant.put(li.group(1), Long.decode(li.group(2)));
                baseBias.remove(li.group(1));
                baseSource.remove(li.group(1));
                continue;
            }

            // Any other definition of a register invalidates what we knew of it.
            Matcher writes = WRITES.matcher(text);
            if (writes.matches()) {
                String dest = writes.group(1);
                constant.remove(dest);
                baseBias.remove(dest);
                baseSource.remove(dest);
            }
        }
        out.println("SUMMARY wanted=0x" + Long.toHexString(wanted) + " hits=" + hits);
        out.close();
        println("WROTE hits=" + hits);
    }
}
