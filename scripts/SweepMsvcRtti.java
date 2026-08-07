// Sweep every read-only block for MSVC vtables and name them from their RTTI.
//
// A vtable is preceded by a pointer to its complete object locator. The locator
// holds a type descriptor at +0x0C whose decorated name lives at +0x08 and
// always begins ".?A", and a class hierarchy descriptor at +0x10 listing the
// bases with their displacements. Nothing here is guessed from adjacency: an
// entry is emitted only when the whole chain resolves and the name has the
// expected prefix. Addresses that look like locators but fail any step are
// counted, not reported as classes.
//
// Read-only. Emits one TSV row per vtable:
//     vtable  class  base_count  bases(name@mdisp;...)
//
// usage: SweepMsvcRtti.java
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.MemoryBlock;

import java.util.ArrayList;
import java.util.List;

public class SweepMsvcRtti extends GhidraScript {

    @Override
    public void run() throws Exception {
        long minimum = Long.MAX_VALUE;
        long maximum = Long.MIN_VALUE;
        List<MemoryBlock> blocks = new ArrayList<>();
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (!block.isInitialized() || !block.isRead()) {
                continue;
            }
            blocks.add(block);
            minimum = Math.min(minimum, block.getStart().getOffset());
            maximum = Math.max(maximum, block.getEnd().getOffset());
        }

        int emitted = 0;
        int rejected = 0;
        for (MemoryBlock block : blocks) {
            if (block.isExecute()) {
                continue;  // vtables live in read-only data
            }
            long start = block.getStart().getOffset();
            long end = block.getEnd().getOffset();
            for (long at = start; at + 4 <= end && !monitor.isCancelled(); at += 4) {
                Long candidate = readU32(at);
                if (candidate == null || candidate < minimum || candidate > maximum) {
                    continue;
                }
                String row = describe(candidate, at + 4, minimum, maximum);
                if (row == null) {
                    if (looksLikeLocator(candidate, minimum, maximum)) {
                        rejected++;
                    }
                    continue;
                }
                println(row);
                emitted++;
            }
        }
        println("AC6_RTTI_SUMMARY vtables=" + emitted + " locator_like_rejected=" + rejected);
    }

    private boolean looksLikeLocator(long locator, long minimum, long maximum) {
        Long signature = readU32(locator);
        Long descriptor = readU32(locator + 0x0C);
        return signature != null && signature <= 1 && descriptor != null
                && descriptor >= minimum && descriptor <= maximum;
    }

    private String describe(long locator, long vtable, long minimum, long maximum) {
        Long signature = readU32(locator);
        if (signature == null || signature > 1) {
            return null;
        }
        Long typeDescriptor = readU32(locator + 0x0C);
        Long hierarchy = readU32(locator + 0x10);
        if (typeDescriptor == null || hierarchy == null) {
            return null;
        }
        if (typeDescriptor < minimum || typeDescriptor > maximum
                || hierarchy < minimum || hierarchy > maximum) {
            return null;
        }
        String name = readName(typeDescriptor);
        if (name == null || !name.startsWith(".?A")) {
            return null;
        }
        // The first vtable slot must be code, or this is not a vtable.
        Long slot = readU32(vtable);
        if (slot == null || !isExecutable(slot)) {
            return null;
        }

        Long count = readU32(hierarchy + 8);
        Long array = readU32(hierarchy + 0x0C);
        StringBuilder bases = new StringBuilder();
        int emitted = 0;
        if (count != null && array != null && count <= 32 && array >= minimum
                && array <= maximum) {
            for (long index = 0; index < count; index++) {
                Long descriptor = readU32(array + index * 4);
                if (descriptor == null) {
                    break;
                }
                Long baseType = readU32(descriptor);
                Long mdisp = readU32(descriptor + 8);
                if (baseType == null || mdisp == null) {
                    break;
                }
                String baseName = readName(baseType);
                if (baseName == null) {
                    break;
                }
                if (emitted > 0) {
                    bases.append(';');
                }
                bases.append(baseName).append('@').append(mdisp);
                emitted++;
            }
        }
        return String.format("AC6_RTTI\t%08x\t%s\t%d\t%s", vtable, name, emitted,
                bases.toString());
    }

    private boolean isExecutable(long address) {
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (block.isExecute() && address >= block.getStart().getOffset()
                    && address <= block.getEnd().getOffset()) {
                return true;
            }
        }
        return false;
    }

    private String readName(long typeDescriptor) {
        StringBuilder text = new StringBuilder();
        for (int index = 0; index < 200; index++) {
            try {
                byte value = currentProgram.getMemory()
                        .getByte(toAddr(typeDescriptor + 8 + index));
                if (value == 0) {
                    return text.length() == 0 ? null : text.toString();
                }
                text.append((char) (value & 0xFF));
            } catch (Exception failure) {
                return null;
            }
        }
        return null;
    }

    private Long readU32(long address) {
        try {
            byte[] bytes = new byte[4];
            currentProgram.getMemory().getBytes(toAddr(address), bytes);
            return ((long) (bytes[0] & 0xFF) << 24) | ((long) (bytes[1] & 0xFF) << 16)
                    | ((long) (bytes[2] & 0xFF) << 8) | (long) (bytes[3] & 0xFF);
        } catch (Exception failure) {
            return null;
        }
    }
}
