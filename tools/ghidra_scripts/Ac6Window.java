// Print a window of instructions around each supplied address. Read-only.
// @category AC6
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class Ac6Window extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        int before = Integer.parseInt(args[1]);
        Set<Long> wanted = new HashSet<>();
        for (int i = 2; i < args.length; ++i) wanted.add(Long.parseLong(args[i], 16));
        List<Instruction> all = new ArrayList<>();
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) all.add(it.next());
        PrintWriter out = new PrintWriter(args[0], "UTF-8");
        for (int i = 0; i < all.size(); ++i) {
            if (!wanted.contains(all.get(i).getAddress().getOffset())) continue;
            Function f = getFunctionContaining(all.get(i).getAddress());
            out.println("### " + all.get(i).getAddress() + " in " +
                    (f == null ? "none" : f.getName() + "@" + f.getEntryPoint()));
            for (int k = Math.max(0, i - before); k <= i; ++k)
                out.println("    " + all.get(k).getAddress() + "  " + all.get(k));
        }
        out.close();
        println("WROTE");
    }
}
