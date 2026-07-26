// Locate bounded Xenos shader containers in initialized program memory.
// This is a read-only diagnostic and does not create symbols or references.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.MemoryBlock;

public class FindXenosShaderContainers extends GhidraScript {
    private static final long HEADER_SIZE = 0x24L;

    private long u32(Address address) throws Exception {
        return Integer.toUnsignedLong(currentProgram.getMemory().getInt(address));
    }

    @Override
    protected void run() throws Exception {
        int count = 0;
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (!block.isInitialized() || block.getSize() < HEADER_SIZE) {
                continue;
            }

            Address cursor = block.getStart();
            Address last = block.getEnd().subtract(HEADER_SIZE - 1L);
            while (cursor.compareTo(last) <= 0) {
                monitor.checkCancelled();
                long flags = u32(cursor);
                if ((flags & 0xffffff00L) == 0x102a1100L) {
                    long virtualSize = u32(cursor.add(0x04L));
                    long physicalSize = u32(cursor.add(0x08L));
                    long constantTableOffset = u32(cursor.add(0x10L));
                    long definitionTableOffset = u32(cursor.add(0x14L));
                    long shaderOffset = u32(cursor.add(0x18L));
                    long field1c = u32(cursor.add(0x1cL));
                    long field20 = u32(cursor.add(0x20L));
                    long totalSize = virtualSize + physicalSize;
                    long remaining = block.getEnd().subtract(cursor) + 1L;
                    boolean bounded = totalSize >= HEADER_SIZE &&
                        totalSize <= remaining && field1c == 0L && field20 == 0L &&
                        constantTableOffset >= HEADER_SIZE &&
                        constantTableOffset < totalSize && shaderOffset >= HEADER_SIZE &&
                        shaderOffset < totalSize &&
                        (definitionTableOffset == 0L ||
                         definitionTableOffset < totalSize);
                    if (bounded) {
                        println(String.format(
                            "%s flags=0x%08x stage=%s virtual=0x%x physical=0x%x " +
                            "constants=0x%x definitions=0x%x shader=0x%x total=0x%x",
                            cursor, flags, (flags & 1L) == 0L ? "pixel" : "vertex",
                            virtualSize, physicalSize, constantTableOffset,
                            definitionTableOffset, shaderOffset, totalSize));
                        count++;
                    }
                }
                cursor = cursor.add(4L);
            }
        }
        println("xenos_shader_containers=" + count);
    }
}
