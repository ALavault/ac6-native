// Find a 32-bit big-endian value as DATA, across every initialised memory block.
//
// The gap this fills: every scan in tools/ghidra_scripts searches instruction
// TEXT. A function reached only through a vtable slot or a dispatch table has no
// instruction mentioning it, so those scans return zero — correctly, and
// uselessly. Cycle 1224 hit exactly that: 0x821B5808 is the general mode-creator
// setter and appears zero times in 852,724 instructions, because it is a table
// entry rather than a call target.
//
// INSTRUMENT_DISCIPLINE.md says a load-bearing negative should be raised to a
// byte-level scan. Until now no tool here did that, so the advice could only be
// followed by hand, and it mostly was not.
//
// Every block is scanned at 4-byte alignment and, separately, at every byte
// offset — an unaligned hit is usually noise but a silent aligned-only scan is
// the kind of partial coverage this file's other entries are about. Both counts
// are reported.
//
// usage: Ac6XenonFindWord OUT hexvalue [more values...]
//        e.g. Ac6XenonFindWord /tmp/out.txt 821b5808 821b54c0

import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;

public class Ac6XenonFindWord extends GhidraScript {
  @Override
  public void run() throws Exception {
    String[] args = getScriptArgs();
    if (args.length < 2) {
      println("usage: Ac6XenonFindWord OUT hexvalue [more values...]");
      return;
    }
    PrintWriter out = new PrintWriter(args[0], "UTF-8");

    int[] wanted = new int[args.length - 1];
    for (int i = 1; i < args.length; ++i) {
      wanted[i - 1] = (int) Long.parseLong(args[i], 16);
    }

    Memory memory = currentProgram.getMemory();
    long bytesScanned = 0;
    long blocksScanned = 0;
    int[] alignedHits = new int[wanted.length];
    int[] unalignedHits = new int[wanted.length];

    for (MemoryBlock block : memory.getBlocks()) {
      if (!block.isInitialized()) continue;
      long size = block.getSize();
      if (size <= 4) continue;
      blocksScanned++;

      byte[] buffer = new byte[(int) Math.min(size, 1 << 22)];
      long done = 0;
      while (done < size) {
        if (monitor.isCancelled()) break;
        int want = (int) Math.min(buffer.length, size - done);
        Address at = block.getStart().add(done);
        int got = block.getBytes(at, buffer, 0, want);
        if (got <= 4) break;
        // Stop four short of the end of each chunk; the overlap below covers
        // values that straddle a chunk boundary.
        for (int i = 0; i + 4 <= got; ++i) {
          int value = ((buffer[i] & 0xFF) << 24) | ((buffer[i + 1] & 0xFF) << 16) |
                      ((buffer[i + 2] & 0xFF) << 8) | (buffer[i + 3] & 0xFF);
          for (int k = 0; k < wanted.length; ++k) {
            if (value != wanted[k]) continue;
            Address hit = at.add(i);
            boolean aligned = (hit.getOffset() & 3L) == 0;
            if (aligned) alignedHits[k]++; else unalignedHits[k]++;
            out.println(args[k + 1] + "  at " + hit + "  in block " + block.getName() +
                        (aligned ? "  aligned" : "  UNALIGNED"));
          }
        }
        bytesScanned += got;
        done += (got >= 4) ? (got - 3) : got;  // overlap 3 bytes across chunks
      }
    }

    StringBuilder summary = new StringBuilder();
    summary.append("blocks=").append(blocksScanned)
           .append(" bytes=").append(bytesScanned);
    for (int k = 0; k < wanted.length; ++k) {
      summary.append("  ").append(args[k + 1]).append("=")
             .append(alignedHits[k]).append("aligned/")
             .append(unalignedHits[k]).append("unaligned");
    }
    out.println(summary);
    out.close();
    println(summary.toString());
    println("WROTE " + args[0]);
  }
}
