// Name a vtable from the MSVC RTTI that precedes it, and list its base classes.
//
// The word immediately before a vtable is its complete object locator. In the
// 32-bit layout this XEX uses, the locator holds
//     +0x00 signature, +0x04 offset, +0x08 cdOffset,
//     +0x0C type descriptor, +0x10 class hierarchy descriptor
// the type descriptor carries the decorated name at +0x08, and the hierarchy
// descriptor holds a base class array at +0x0C whose entries begin with their
// own type descriptor. Read-only.
//
// usage: DescribeVtableRtti.java VTABLE[,VTABLE...]
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;

public class DescribeVtableRtti extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            printerr("usage: DescribeVtableRtti.java VTABLE[,VTABLE...]");
            return;
        }
        for (String piece : args[0].split(",")) {
            describe(Long.decode(piece.trim()));
        }
    }

    private void describe(long vtable) {
        try {
            long locator = readU32(vtable - 4);
            println(String.format("AC6_VTABLE 0x%08X locator=0x%08X", vtable, locator));
            if (locator == 0) {
                return;
            }
            long typeDescriptor = readU32(locator + 0x0C);
            long hierarchy = readU32(locator + 0x10);
            println(String.format(
                    "  offset=0x%X cd_offset=0x%X type_descriptor=0x%08X name=%s",
                    readU32(locator + 4), readU32(locator + 8), typeDescriptor,
                    readName(typeDescriptor)));
            long count = readU32(hierarchy + 8);
            long array = readU32(hierarchy + 0x0C);
            println(String.format("  hierarchy=0x%08X bases=%d", hierarchy, count));
            for (long index = 0; index < count && index < 32; index++) {
                long descriptor = readU32(array + index * 4);
                long baseType = readU32(descriptor);
                println(String.format("    base[%d] descriptor=0x%08X name=%s mdisp=0x%X",
                        index, descriptor, readName(baseType),
                        readU32(descriptor + 8)));
            }
        } catch (Exception failure) {
            printerr(String.format("0x%08X: %s", vtable, failure.getMessage()));
        }
    }

    private String readName(long typeDescriptor) throws Exception {
        StringBuilder text = new StringBuilder();
        Address cursor = toAddr(typeDescriptor + 8);
        for (int index = 0; index < 160; index++) {
            byte value = currentProgram.getMemory().getByte(cursor.add(index));
            if (value == 0) {
                break;
            }
            text.append((char) (value & 0xFF));
        }
        return text.toString();
    }

    private long readU32(long address) throws Exception {
        byte[] bytes = new byte[4];
        currentProgram.getMemory().getBytes(toAddr(address), bytes);
        return ((long) (bytes[0] & 0xFF) << 24) | ((long) (bytes[1] & 0xFF) << 16)
                | ((long) (bytes[2] & 0xFF) << 8) | (long) (bytes[3] & 0xFF);
    }
}
