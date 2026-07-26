// Finds MSVC Complete Object Locators and vtables for exact TypeDescriptors.
// Read-only diagnostic for the qualified big-endian AC6 XEX.
// @category AC6

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.MemoryBlock;

public class FindMsvcRttiVtables extends GhidraScript {
    private long readU32(Address address) throws Exception {
        byte[] bytes = new byte[4];
        currentProgram.getMemory().getBytes(address, bytes);
        return ((long)(bytes[0] & 0xff) << 24) |
               ((long)(bytes[1] & 0xff) << 16) |
               ((long)(bytes[2] & 0xff) << 8) |
               (long)(bytes[3] & 0xff);
    }

    private List<MemoryBlock> readableDataBlocks() {
        List<MemoryBlock> blocks = new ArrayList<>();
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (block.isInitialized() && block.isRead() && !block.isExecute()) {
                blocks.add(block);
            }
        }
        return blocks;
    }

    @Override
    protected void run() throws Exception {
        if (getScriptArgs().length == 0) {
            throw new IllegalArgumentException(
                "usage: FindMsvcRttiVtables <type-descriptor>...");
        }
        Set<Long> targets = new HashSet<>();
        for (String argument : getScriptArgs()) {
            targets.add(Long.decode(argument) & 0xffffffffL);
        }

        List<MemoryBlock> blocks = readableDataBlocks();
        for (long typeDescriptor : targets) {
            List<Address> locators = new ArrayList<>();
            for (MemoryBlock block : blocks) {
                Address current = block.getStart();
                Address last = block.getEnd().subtract(0x13);
                while (current.compareTo(last) <= 0) {
                    if ((readU32(current) == 0 || readU32(current) == 1) &&
                        readU32(current.add(0xc)) == typeDescriptor) {
                        locators.add(current);
                    }
                    current = current.add(4);
                }
            }

            for (Address locator : locators) {
                long locatorValue = locator.getOffset() & 0xffffffffL;
                boolean foundVtable = false;
                for (MemoryBlock block : blocks) {
                    Address current = block.getStart();
                    Address last = block.getEnd().subtract(3);
                    while (current.compareTo(last) <= 0) {
                        if (readU32(current) == locatorValue) {
                            Address vtable = current.add(4);
                            println(String.format(
                                "type=0x%08x col=%s vtable=%s",
                                typeDescriptor, locator, vtable));
                            foundVtable = true;
                        }
                        current = current.add(4);
                    }
                }
                if (!foundVtable) {
                    println(String.format(
                        "type=0x%08x col=%s vtable=none",
                        typeDescriptor, locator));
                }
            }
            if (locators.isEmpty()) {
                println(String.format(
                    "type=0x%08x col=none vtable=none", typeDescriptor));
            }
        }
    }
}
