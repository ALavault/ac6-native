// A substring scan that first FORCES disassembly over the range, closing the
// blind spot cycle 1220 explained.
//
// Ac6XenonRefs iterates getListing().getInstructions(true), which yields only
// instructions Ghidra's auto-analysis already created. In this project that is
// 786,122 of the 859,595 instructions in .text - 91.5%. The missing 8.5% is not
// random: it is every function analysis never reached, and a negative taken over
// it is indistinguishable from a negative over a program that lacks the code.
//
// It cost a real finding. A search for "0x1ad8" returned zero while
// 821b54c8  lwz r11,0x1ad8(r11) exists and contains it; a bare "1ad8" returned
// eleven hits in the same run, so the matcher was fine. The instruction was
// simply not in the listing.
//
// This script walks the range in 4-byte steps, disassembles anything the listing
// does not already have, and only then scans. It reports both counts, so the
// output states its own denominator - which the rule in INSTRUMENT_DISCIPLINE.md
// asks for and which no scan here was doing.
//
// usage: Ac6XenonForceScan OUT startHex endHex pattern [more patterns...]
//        e.g. Ac6XenonForceScan /tmp/out.txt 82090000 823d772c 0x1ad8
//
// Patterns are SUBSTRINGS, not regexes - the same trap Ac6XenonRefs warns about,
// repeated here because it is repeated in practice.

import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

public class Ac6XenonForceScan extends GhidraScript {
  @Override
  public void run() throws Exception {
    String[] args = getScriptArgs();
    if (args.length < 4) {
      println("usage: Ac6XenonForceScan OUT startHex endHex pattern [patterns...]");
      return;
    }
    PrintWriter out = new PrintWriter(args[0], "UTF-8");

    for (int i = 3; i < args.length; ++i) {
      if (args[i].matches(".*[\\^$\\\\\\[\\]|+*?].*")) {
        println("WARNING: pattern \"" + args[i] + "\" contains regex syntax; " +
                "this matcher is SUBSTRING only. A zero here is not a negative.");
        out.println("# WARNING regex-looking pattern matched literally: " + args[i]);
      }
    }

    long start = Long.parseLong(args[1], 16);
    long end = Long.parseLong(args[2], 16);
    Address from = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(start);
    Address to = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(end);

    long already = 0, forced = 0, failed = 0;
    for (long p = start; p < end; p += 4) {
      if (monitor.isCancelled()) break;
      Address a = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(p);
      if (getInstructionAt(a) != null) {
        already++;
        continue;
      }
      // disassemble() is a no-op when the bytes are already claimed by data; a
      // failure here is information, not an error, so it is counted rather than
      // thrown.
      if (disassemble(a) && getInstructionAt(a) != null) {
        forced++;
      } else {
        failed++;
      }
    }

    long total = 0, hits = 0;
    InstructionIterator it =
        currentProgram.getListing().getInstructions(new AddressSet(from, to), true);
    while (it.hasNext()) {
      Instruction ins = it.next();
      total++;
      String text = ins.toString();
      for (int i = 3; i < args.length; ++i) {
        if (text.toLowerCase().contains(args[i].toLowerCase())) {
          Function f = getFunctionContaining(ins.getAddress());
          out.println(args[i] + "  at " + ins.getAddress() + "  in " +
                      (f == null ? "none" : f.getName() + "@" + f.getEntryPoint()) +
                      "  " + text);
          hits++;
        }
      }
    }

    // The denominator, printed with the result rather than left to the reader.
    String summary = "scanned=" + total + " already_listed=" + already +
                     " forced=" + forced + " undisassemblable=" + failed +
                     " hits=" + hits;
    out.println(summary);
    out.close();
    println(summary);
    println("WROTE " + args[0]);
  }
}
