// Read-only assertions for the AC6 PAL MATE material -> ShaderContext path.
// This contract proves the key, active parameter-application boundary and the
// selected-material -> draw-request edge.  The request carries the selected
// material at +0x24 and a distinct shader-context key at +0x08; it deliberately
// does not claim those two keys are equal.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyMateShaderContextBindingContracts extends GhidraScript {
    private int assertions;

    private void requireInstruction(long value, String mnemonic, String fragment)
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
        assertions++;
        println("AC6_MATE_SHADER_CONTEXT_CONTRACT=" + address + " " + instruction);
    }

    private void requireWord(long addressValue, long expected) throws Exception {
        Address address = toAddr(addressValue);
        long actual = currentProgram.getMemory().getInt(address) & 0xffffffffL;
        if (actual != expected) {
            throw new IllegalStateException(String.format(
                "%s expected 0x%08x but found 0x%08x", address, expected, actual));
        }
        assertions++;
        println(String.format("AC6_MATE_SHADER_CONTEXT_WORD=%s 0x%08x", address, actual));
    }

    @Override
    public void run() throws Exception {
        // Lazy resolver for the material header: material+0 is the registry
        // key, +4 caches the resolved context and bit 0x4000 at +8 records it.
        requireInstruction(0x8233ef9cL, "lhz", "r11,0x8(r31)");
        requireInstruction(0x8233efa0L, "rlwinm.", "0x11,0x11");
        requireInstruction(0x8233efacL, "lwz", "r4,0x0(r31)");
        requireInstruction(0x8233efb4L, "subi", "r3,r11,0x3480");
        requireInstruction(0x8233efb8L, "bl", "0x8233f2b0");
        requireInstruction(0x8233efe8L, "stw", "r11,0x4(r31)");
        requireInstruction(0x8233efecL, "bl", "0x8233eda8");
        requireInstruction(0x8233eff4L, "ori", "r11,r11,0x4000");
        requireInstruction(0x8233eff8L, "sth", "r11,0x8(r31)");

        // The material fixup walks texture subrecords at +0x20, stride 0x18,
        // and resolves this same material after the subordinate records.
        requireInstruction(0x82355330L, "lhz", "r11,0xa(r29)");
        requireInstruction(0x8235532cL, "addi", "r30,r29,0x20");
        requireInstruction(0x8235534cL, "addi", "r30,r30,0x18");
        requireInstruction(0x82355358L, "mulli", "r11,r11,0x18");
        requireInstruction(0x82355344L, "bl", "0x8233ee40");
        requireInstruction(0x823553b0L, "or", "r3,r29,r29");
        requireInstruction(0x823553b4L, "bl", "0x8233ef88");

        // Active selector uses r10 to choose one of the material pointers and
        // falls back to the low two bits at owner+0x22 when that slot is null.
        requireInstruction(0x82362a44L, "addi", "r11,r10,0x4");
        requireInstruction(0x82362a4cL, "rlwinm", "r11,r11,0x2");
        requireInstruction(0x82362a60L, "lwzx", "r26,r11,r31");
        requireInstruction(0x82362a74L, "lhz", "r11,0x22(r31)");
        requireInstruction(0x82362a84L, "lwzx", "r26,r11,r31");
        requireInstruction(0x82362adcL, "lhz", "r11,0x8(r26)");
        requireInstruction(0x82362ae8L, "lwz", "r30,0x4(r26)");
        requireInstruction(0x82362af4L, "bl", "0x8233ef88");

        // The selected context receives the current state through vslot +0x24.
        // In ShaderContextXenon that slot is 0x8234b870; +0x28 remains the
        // distinct shader-publishing method at 0x82350318.
        requireWord(0x820126f0L, 0x8234b870L);
        requireWord(0x820126f4L, 0x82350318L);
        requireInstruction(0x82362b10L, "stw", "r24,0x60(r1)");
        requireInstruction(0x82362b18L, "lwz", "r11,0x24(r11)");
        requireInstruction(0x82362b20L, "bctrl", "bctrl");

        // The +0x24 implementation traverses both parameter lists and invokes
        // each child vslot +8 with the same state object.
        requireInstruction(0x8234b884L, "lwz", "r31,0x10(r30)");
        requireInstruction(0x8234b898L, "lwz", "r11,0x8(r11)");
        requireInstruction(0x8234b8b0L, "lwz", "r31,0x14(r30)");
        requireInstruction(0x8234b8c4L, "lwz", "r11,0x8(r11)");

        // A sibling render path repeats the same resolve and +0x24 dispatch.
        requireInstruction(0x82362d54L, "bl", "0x8233ef88");
        requireInstruction(0x82362d78L, "lwz", "r11,0x24(r11)");
        requireInstruction(0x82362d80L, "bctrl", "bctrl");

        // The first active selector also constructs a draw request from the
        // selected material.  r6 is stored at request+0x24, while request+0x08
        // is independently initialized to zero.  The material is then passed
        // to the request-chain owner; this is a causal edge, not key equality.
        requireInstruction(0x82362c3cL, "or", "r6,r26,r26");
        requireInstruction(0x82362c4cL, "bl", "0x82363f58");
        requireInstruction(0x82362c58L, "or", "r3,r26,r26");
        requireInstruction(0x82362c5cL, "bl", "0x8233ed10");
        requireInstruction(0x82363f64L, "stw", "r4,0x1c(r3)");
        requireInstruction(0x82363f68L, "stw", "r5,0x20(r3)");
        requireInstruction(0x82363f70L, "stw", "r6,0x24(r3)");
        requireInstruction(0x82363f78L, "stw", "r7,0x28(r3)");
        requireInstruction(0x82363f7cL, "stw", "r10,0x0(r3)");
        requireInstruction(0x82363f80L, "stw", "r11,0x4(r3)");
        requireInstruction(0x82363f84L, "stw", "r11,0x8(r3)");
        requireInstruction(0x82363f88L, "stw", "r11,0xc(r3)");

        // The request vtable's draw slot (+0x14) resolves the independent key
        // at request+0x08, invokes ShaderContext vslot +0x28 with request+0x28,
        // and then reaches all three indexed-draw branches.  No writer joining
        // request+0x08 to material+0 is asserted here.
        requireWord(0x82015410L, 0x82364980L);
        requireInstruction(0x82364b44L, "lwz", "r29,0x8(r30)");
        requireInstruction(0x82364b50L, "subi", "r3,r11,0x3480");
        requireInstruction(0x82364b58L, "bl", "0x8233f2b0");
        requireInstruction(0x82364b60L, "lwz", "r4,0x28(r30)");
        requireInstruction(0x82364b68L, "lwz", "r11,0x28(r11)");
        requireInstruction(0x82364b70L, "bctrl", "bctrl");
        requireInstruction(0x82364cb0L, "bl", "0x821df2c0");
        requireInstruction(0x82364d10L, "bl", "0x821df2c0");
        requireInstruction(0x82364d3cL, "bl", "0x821df2c0");

        println("AC6_MATE_SHADER_CONTEXT_ASSERTIONS=" + assertions);
    }
}
