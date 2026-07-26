// Scan mapped executable memory for PPC D-form loads/stores using an exact
// signed displacement. This also covers code that Ghidra has not placed in a
// function or disassembled persistently.
//
// Usage: FindPpcMemoryDisplacement.java <start> <end-exclusive> <displacement>
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryAccessException;

public class FindPpcMemoryDisplacement extends GhidraScript {
    private static String opcodeName(int opcode) {
        switch (opcode) {
            case 32: return "lwz";
            case 33: return "lwzu";
            case 34: return "lbz";
            case 35: return "lbzu";
            case 36: return "stw";
            case 37: return "stwu";
            case 38: return "stb";
            case 39: return "stbu";
            case 40: return "lhz";
            case 41: return "lhzu";
            case 42: return "lha";
            case 43: return "lhau";
            case 44: return "sth";
            case 45: return "sthu";
            case 48: return "lfs";
            case 49: return "lfsu";
            case 50: return "lfd";
            case 51: return "lfdu";
            case 52: return "stfs";
            case 53: return "stfsu";
            case 54: return "stfd";
            case 55: return "stfdu";
            default: return null;
        }
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 3) {
            throw new IllegalArgumentException(
                "usage: FindPpcMemoryDisplacement <start> <end-exclusive> <displacement>");
        }

        long startValue = Long.decode(args[0]);
        long endValue = Long.decode(args[1]);
        int expected = (short) (long) Long.decode(args[2]);
        Memory memory = currentProgram.getMemory();

        for (long value = startValue; value < endValue; value += 4) {
            Address address = toAddr(value);
            int word;
            try {
                word = memory.getInt(address);
            } catch (MemoryAccessException ignored) {
                continue;
            }

            int opcode = word >>> 26;
            String mnemonic = opcodeName(opcode);
            if (mnemonic == null || (short) word != expected) {
                continue;
            }

            int sourceOrTarget = (word >>> 21) & 0x1f;
            int base = (word >>> 16) & 0x1f;
            Function function = currentProgram.getFunctionManager()
                .getFunctionContaining(address);
            String owner = function == null
                ? "<no-function>"
                : function.getEntryPoint() + " " + function.getName();
            println(String.format(
                "%s %s %s r%d,0x%04x(r%d) word=0x%08x",
                address, owner, mnemonic, sourceOrTarget, word & 0xffff, base, word));
        }
    }
}
