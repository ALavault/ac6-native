// Census of CALLOTHER p-code operations, which is the size of the gap between
// "Ghidra disassembles this" and "Ghidra can micro-execute this".
//
// A SLEIGH module may decode an instruction and still give it no executable
// semantics, emitting a CALLOTHER the emulator refuses with
// UnimplementedCallOtherException. The Xenon module does exactly that for parts
// of VMX128: cycle 1294 reached 142 steps into 0x822A1E80 before
// `loadVectorLeftIndexed128` stopped it.
//
// So the question "can the harness execute gameplay code" is not about VMX128
// as a family. It is about a finite list of named operations, and this counts
// them so the list is a measurement rather than an impression.
//
// Usage:
//   -postScript Ac6CallOtherCensus.java START END OUT_TSV
// Read-only. Run with -readOnly -noanalysis.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Language;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.pcode.PcodeOp;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public class Ac6CallOtherCensus extends GhidraScript {

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 3) {
            throw new IllegalArgumentException(
                "usage: Ac6CallOtherCensus START END OUT_TSV");
        }
        Address start = toAddr(Long.decode(args[0]) & 0xffffffffL);
        Address end = toAddr(Long.decode(args[1]) & 0xffffffffL);
        Language language = currentProgram.getLanguage();

        Map<String, Integer> counts = new LinkedHashMap<>();
        Map<String, String> firstSite = new LinkedHashMap<>();
        long instructions = 0;

        for (Instruction instruction : currentProgram.getListing().getInstructions(start, true)) {
            if (instruction.getAddress().compareTo(end) >= 0) {
                break;
            }
            instructions++;
            for (PcodeOp op : instruction.getPcode()) {
                if (op.getOpcode() != PcodeOp.CALLOTHER) {
                    continue;
                }
                // Operand 0 of a CALLOTHER is the index into the language's
                // user-defined op table; the name is what the emulator reports.
                int index = (int) op.getInput(0).getOffset();
                String name = language.getUserDefinedOpName(index);
                if (name == null) {
                    name = "userop_" + index;
                }
                counts.merge(name, 1, Integer::sum);
                firstSite.putIfAbsent(name, instruction.getAddress().toString()
                    + "\t" + instruction.getMnemonicString());
            }
        }

        List<Map.Entry<String, Integer>> ranked = new ArrayList<>(counts.entrySet());
        ranked.sort((left, right) -> Integer.compare(right.getValue(), left.getValue()));

        try (PrintWriter out = new PrintWriter(args[2])) {
            out.println("# CALLOTHER p-code operations emitted over "
                + start + ".." + end + ", " + instructions + " instructions.");
            out.println("# Produced by scripts/Ac6CallOtherCensus.java. Read-only, no oracle.");
            out.println("# A name here is an instruction the emulator will refuse unless the");
            out.println("# harness registers a behaviour for it.");
            out.println("# columns: pcodeop count first_site mnemonic");
            for (Map.Entry<String, Integer> entry : ranked) {
                out.println(entry.getKey() + "\t" + entry.getValue() + "\t"
                    + firstSite.get(entry.getKey()));
            }
        }
        println("AC6_CALLOTHER_CENSUS instructions=" + instructions
            + " distinct_ops=" + counts.size()
            + " total_sites=" + counts.values().stream().mapToInt(Integer::intValue).sum()
            + " out=" + args[2]);
    }
}
