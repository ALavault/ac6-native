// Disassemble the undecoded regions of .text in the NON-CANONICAL Xenon scratch
// corpus, then re-measure coverage.
//
// Every sweep in this series reads only decoded instructions, and cycle 1140
// measured that 13.77% of .text is not decoded. Cycle 1122 showed what that
// costs: a jump table and its target bodies sat as data, and disassembling
// 2,416 bytes produced a caller that a sweep of 755,392 instructions had missed.
//
// This writes to the scratch project only. The canonical project is never
// opened for writing.
// @category AC6
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.mem.MemoryBlock;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

public class Ac6FillGaps extends GhidraScript {
    @Override
    public void run() throws Exception {
        if (!currentProgram.getDomainFile().getPathname().contains("default.xex")) return;
        PrintWriter out = new PrintWriter(getScriptArgs()[0], "UTF-8");

        AddressSet decoded = new AddressSet();
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) { Instruction i = it.next(); decoded.add(i.getMinAddress(), i.getMaxAddress()); }

        MemoryBlock text = null;
        for (MemoryBlock b : currentProgram.getMemory().getBlocks())
            if (b.isExecute() && b.getName().equals(".text")) text = b;
        if (text == null) { out.println("no .text"); out.close(); return; }

        AddressSet block = new AddressSet(text.getStart(), text.getEnd());
        long before = decoded.intersect(block).getNumAddresses();
        List<Address> starts = new ArrayList<>();
        for (AddressRange r : block.subtract(decoded).getAddressRanges()) {
            if (r.getLength() < 8) continue;
            long a = r.getMinAddress().getOffset();
            if ((a & 3) != 0) a = (a + 3) & ~3L;      // instructions are word aligned
            starts.add(r.getMinAddress().getNewAddress(a));
        }
        out.println("gaps_attempted " + starts.size());
        int ok = 0;
        for (Address a : starts) {
            if (monitor.isCancelled()) break;
            try { if (disassemble(a)) ok++; } catch (Exception ignored) { }
        }
        out.println("gaps_disassembled " + ok);

        AddressSet after = new AddressSet();
        it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) { Instruction i = it.next(); after.add(i.getMinAddress(), i.getMaxAddress()); }
        long now = after.intersect(block).getNumAddresses();
        long total = block.getNumAddresses();
        out.println(String.format("decoded_before %d (%.2f%%)", before, 100.0 * before / total));
        out.println(String.format("decoded_after  %d (%.2f%%)", now, 100.0 * now / total));
        out.println("gained_bytes " + (now - before));
        out.close();
        println("WROTE gained=" + (now - before));
    }
}
