// Finds PPC relative/absolute branch encodings in raw executable memory.
// Unlike FindPpcBranchesTo, this does not require Ghidra to have defined an
// Instruction at the candidate address. Read-only diagnostic script.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.MemoryBlock;

public class FindPpcRawBranchesTo extends GhidraScript {
    private static long u32(byte[] bytes, int offset) {
        return ((long)(bytes[offset] & 0xff) << 24) |
               ((long)(bytes[offset + 1] & 0xff) << 16) |
               ((long)(bytes[offset + 2] & 0xff) << 8) |
               (long)(bytes[offset + 3] & 0xff);
    }

    private static long signExtend(long value, int bits) {
        long sign = 1L << (bits - 1);
        return (value ^ sign) - sign;
    }

    private static long branchTarget(long instructionAddress, long word) {
        long opcode = word >>> 26;
        long displacement;
        if (opcode == 18) {
            displacement = signExtend((word >>> 2) & 0x00ffffffL, 24) << 2;
        } else if (opcode == 16) {
            displacement = signExtend((word >>> 2) & 0x3fffL, 14) << 2;
        } else {
            return Long.MIN_VALUE;
        }
        return ((word & 0x2L) != 0)
            ? displacement & 0xffffffffL
            : (instructionAddress + displacement) & 0xffffffffL;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length == 0) {
            throw new IllegalArgumentException(
                "usage: FindPpcRawBranchesTo <address>...");
        }
        long[] targets = new long[args.length];
        for (int i = 0; i < args.length; ++i) {
            targets[i] = Long.decode(args[i]) & 0xffffffffL;
        }

        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (!block.isExecute() || !block.isInitialized()) {
                continue;
            }
            long length = block.getSize();
            byte[] bytes = new byte[(int)Math.min(length, 1 << 20)];
            for (long base = 0; base < length; base += bytes.length) {
                int count = (int)Math.min(bytes.length, length - base);
                Address address = block.getStart().add(base);
                currentProgram.getMemory().getBytes(address, bytes, 0, count);
                for (int offset = 0; offset + 4 <= count; offset += 4) {
                    long word = u32(bytes, offset);
                    long target = branchTarget(address.getOffset() + offset, word);
                    if (target == Long.MIN_VALUE) {
                        continue;
                    }
                    for (long wanted : targets) {
                        if (target == wanted) {
                            println(address.add(offset) + " -> 0x" +
                                Long.toHexString(target) + " raw=0x" +
                                Long.toHexString(word));
                            break;
                        }
                    }
                }
            }
        }
    }
}
