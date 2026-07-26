// Read-only assertions for the AC6 PAL MATE draw-request key lifecycle.
//
// The selected MATE material is stored at request+0x24.  The request's shader
// key at +0x08 is independently initialized to zero; both the direct and
// sorted queue paths dispatch the request's draw slot without writing that
// field.  The draw slot resolves key zero as a ShaderContext and invokes its
// +0x28 shader-publication method before the indexed draw.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyMateDrawRequestKeyLifecycleContracts extends GhidraScript {
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
        println("AC6_MATE_REQUEST_KEY_CONTRACT=" + address + " " + instruction);
    }

    private void requireWord(long addressValue, long expected) throws Exception {
        Address address = toAddr(addressValue);
        long actual = currentProgram.getMemory().getInt(address) & 0xffffffffL;
        if (actual != expected) {
            throw new IllegalStateException(String.format(
                "%s expected 0x%08x but found 0x%08x", address, expected, actual));
        }
        assertions++;
        println(String.format("AC6_MATE_REQUEST_KEY_WORD=%s 0x%08x", address, actual));
    }

    @Override
    public void run() throws Exception {
        // Selected material -> request.  The constructor stores it at +0x24,
        // but initializes the independent request shader key at +0x08 to zero.
        requireInstruction(0x82362c3cL, "or", "r6,r26,r26");
        requireInstruction(0x82362c4cL, "bl", "0x82363f58");
        requireInstruction(0x82363f70L, "stw", "r6,0x24(r3)");
        requireInstruction(0x82363f74L, "li", "r11,0x0");
        requireInstruction(0x82363f84L, "stw", "r11,0x8(r3)");
        requireInstruction(0x82362c50L, "or", "r4,r3,r3");
        requireInstruction(0x82362c58L, "or", "r3,r26,r26");
        requireInstruction(0x82362c5cL, "bl", "0x8233ed10");
        requireWord(0x82015410L, 0x82364980L);

        // The material flags select three direct queues or one sorted queue.
        // r31 is the request pointer throughout this dispatcher.
        requireInstruction(0x8233ed20L, "or", "r31,r4,r4");
        requireInstruction(0x8233ed28L, "lhz", "r11,0x8(r30)");
        requireInstruction(0x8233ed2cL, "rlwinm.", "0x1f,0x1f");
        requireInstruction(0x8233ed40L, "lhz", "r11,0x8(r30)");
        requireInstruction(0x8233ed44L, "rlwinm.", "0x1e,0x1e");
        requireInstruction(0x8233ed54L, "or", "r4,r31,r31");
        requireInstruction(0x8233ed60L, "bl", "0x8233ac68");
        requireInstruction(0x8233ed68L, "bl", "0x8233ac58");
        requireInstruction(0x8233ed7cL, "or", "r4,r31,r31");
        requireInstruction(0x8233ed84L, "bl", "0x8233ac48");
        requireInstruction(0x8233ed90L, "or", "r4,r31,r31");
        requireInstruction(0x8233ed98L, "bl", "0x8233ac38");

        // Direct queue: only the previous request's +4 link and manager tail
        // change.  The consumer obtains the head and immediately calls +0x14.
        requireInstruction(0x823461c8L, "or", "r30,r4,r4");
        requireInstruction(0x823461d8L, "lwz", "r3,0x4(r31)");
        requireInstruction(0x823461dcL, "or", "r4,r30,r30");
        requireInstruction(0x823461e4L, "lwz", "r11,0x10(r11)");
        requireInstruction(0x823461ecL, "bctrl", "bctrl");
        requireInstruction(0x823461f4L, "stw", "r30,0x4(r31)");
        requireInstruction(0x82346218L, "lwz", "r31,0x8(r3)");
        requireInstruction(0x8234622cL, "lwz", "r11,0x14(r11)");
        requireInstruction(0x82346234L, "bctrl", "bctrl");
        requireInstruction(0x82346240L, "lwz", "r11,0xc(r11)");

        // Sorted queue: request and sort float are stored in an external
        // eight-byte record.  The consumer reads the request pointer from that
        // record and immediately dispatches +0x14.
        requireInstruction(0x82346408L, "or", "r30,r4,r4");
        requireInstruction(0x82346424L, "lwz", "r11,0xc(r31)");
        requireInstruction(0x82346428L, "stw", "r30,0x0(r11)");
        requireInstruction(0x82346430L, "stfs", "f31,0x4(r11)");
        requireInstruction(0x8234643cL, "addi", "r10,r10,0x8");
        requireInstruction(0x82346470L, "lwz", "r31,0x8(r3)");
        requireInstruction(0x82346478L, "lwz", "r11,0x0(r3)");
        requireInstruction(0x8234647cL, "lwz", "r11,0x14(r11)");
        requireInstruction(0x82346484L, "bctrl", "bctrl");
        requireInstruction(0x8234648cL, "lwz", "r3,0x0(r31)");

        // Draw slot: request+0x08 is the registry key, +0x28 is the bind
        // argument, and all three terminal branches call indexed draw.
        requireInstruction(0x82364b44L, "lwz", "r29,0x8(r30)");
        requireInstruction(0x82364b54L, "or", "r4,r29,r29");
        requireInstruction(0x82364b58L, "bl", "0x8233f2b0");
        requireInstruction(0x82364b60L, "lwz", "r4,0x28(r30)");
        requireInstruction(0x82364b68L, "lwz", "r11,0x28(r11)");
        requireInstruction(0x82364b70L, "bctrl", "bctrl");
        requireInstruction(0x82364cb0L, "bl", "0x821df2c0");
        requireInstruction(0x82364d10L, "bl", "0x821df2c0");
        requireInstruction(0x82364d3cL, "bl", "0x821df2c0");

        println("AC6_MATE_REQUEST_KEY_ASSERTIONS=" + assertions);
    }
}
