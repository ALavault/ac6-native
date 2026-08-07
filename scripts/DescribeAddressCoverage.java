// For each address, say what holds it: a defined function, a .pdata extent, an
// instruction, or nothing.
//
// A call site that is not inside a bounded function can fail three different
// ways, and the fix differs for each: the linker may never have recorded the
// function (no .pdata entry), the entry may exist without a Ghidra function, or
// the function may exist but stop short of the address. This prints which.
// Read-only.
//
// usage: DescribeAddressCoverage.java ADDR[,ADDR...]
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

import java.util.ArrayList;
import java.util.List;

public class DescribeAddressCoverage extends GhidraScript {

    private static final long PDATA_START = 0x82079e00L;
    private static final int PDATA_BYTES = 65968;

    private record Entry(long begin, long length) {}

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            printerr("usage: DescribeAddressCoverage.java ADDR[,ADDR...]");
            return;
        }
        List<Entry> table = readPdata();
        println("AC6_PDATA_ENTRIES " + table.size());

        int inFunction = 0;
        int inPdataOnly = 0;
        int bare = 0;
        for (String piece : args[0].split(",")) {
            long value = Long.decode(piece.trim());
            Address address = toAddr(value);
            Function function = currentProgram.getFunctionManager()
                    .getFunctionContaining(address);
            Entry entry = containing(table, value);
            Instruction instruction =
                    currentProgram.getListing().getInstructionContaining(address);

            String functionText = function == null ? "none"
                    : function.getEntryPoint() + "-"
                      + function.getBody().getMaxAddress();
            String entryText = entry == null ? "none"
                    : String.format("0x%08X+0x%X", entry.begin(), entry.length());
            println(String.format(
                    "AC6_COVERAGE address=%s function=%s pdata=%s instruction=%s",
                    address, functionText, entryText,
                    instruction == null ? "none" : instruction.getAddress().toString()));
            if (function != null) {
                inFunction++;
            } else if (entry != null) {
                inPdataOnly++;
            } else {
                bare++;
            }
        }
        println(String.format(
                "AC6_COVERAGE_SUMMARY in_function=%d pdata_without_function=%d neither=%d",
                inFunction, inPdataOnly, bare));
    }

    private Entry containing(List<Entry> table, long address) {
        for (Entry entry : table) {
            if (address >= entry.begin() && address < entry.begin() + entry.length()) {
                return entry;
            }
        }
        return null;
    }

    private List<Entry> readPdata() throws Exception {
        List<Entry> table = new ArrayList<>();
        byte[] bytes = new byte[PDATA_BYTES];
        currentProgram.getMemory().getBytes(toAddr(PDATA_START), bytes);
        for (int offset = 0; offset + 8 <= bytes.length; offset += 8) {
            long begin = readU32(bytes, offset);
            long packed = readU32(bytes, offset + 4);
            if (begin == 0 && packed == 0) {
                continue;
            }
            // bits 8..29 hold the function length in words.
            long words = (packed >>> 8) & 0x3FFFFF;
            table.add(new Entry(begin, words * 4));
        }
        return table;
    }

    private long readU32(byte[] bytes, int offset) {
        return ((long) (bytes[offset] & 0xFF) << 24)
                | ((long) (bytes[offset + 1] & 0xFF) << 16)
                | ((long) (bytes[offset + 2] & 0xFF) << 8)
                | (long) (bytes[offset + 3] & 0xFF);
    }
}
