// Read-only assertions for the qualified AC6 PAL TextureContext backing store.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyTextureContextBackingStore extends GhidraScript {
    private void require(long value, String mnemonic, String fragment)
            throws Exception {
        Address address = toAddr(value);
        Instruction instruction = currentProgram.getListing().getInstructionAt(address);
        if (instruction == null) {
            Disassembler.getDisassembler(currentProgram, monitor, null)
                .disassemble(address, null);
            instruction = currentProgram.getListing().getInstructionAt(address);
        }
        if (instruction == null || !mnemonic.equals(instruction.getMnemonicString()) ||
                !instruction.toString().contains(fragment)) {
            throw new IllegalStateException(address + " expected " + mnemonic +
                " containing " + fragment + " but found " +
                (instruction == null ? "<none>" : instruction));
        }
        println("AC6_TEXTURE_BACKING=" + address + " " + instruction);
    }

    @Override
    public void run() throws Exception {
        // Allocate-backed initialization paths.
        require(0x8234ec94L, "bl", "8233be20");
        require(0x8234eca0L, "stw", "r4,0x50(r31)");
        require(0x8234ee08L, "bl", "8233be20");
        require(0x8234ee10L, "stw", "r4,0x50(r31)");
        require(0x8234fc04L, "bl", "8233be20");
        require(0x8234fc10L, "stw", "r4,0x50(r30)");

        // Source-backed initialization paths.
        require(0x8234eea8L, "bl", "8234b268");
        require(0x8234eeb4L, "stw", "r4,0x50(r31)");
        require(0x8234f128L, "bl", "8234b268");
        require(0x8234f134L, "stw", "r4,0x50(r31)");
        require(0x8234ff08L, "bl", "8234b268");
        require(0x8234ff14L, "stw", "r4,0x50(r31)");

        // Descriptor relocation, data transfer, getter and release.
        require(0x8234eca4L, "bl", "821fc070");
        require(0x8234ed50L, "lwz", "r8,0x50(r31)");
        require(0x8234ed78L, "bl", "821fca48");
        require(0x8234ed88L, "lwz", "r11,0x50(r3)");
        require(0x8234ed98L, "stw", "r11,0x0(r6)");
        require(0x8234ef6cL, "lwz", "r3,0x50(r31)");
        require(0x8234ef90L, "stw", "r11,0x50(r31)");
    }
}
