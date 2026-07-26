// Read-only assertions for the AC6 PAL ShaderContextXenon factory paths.
// These checks qualify object construction and shader-description inputs;
// they deliberately do not infer a MATE technique or permutation.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyShaderContextFactoryContracts extends GhidraScript {
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
        println("AC6_SHADER_CONTEXT_FACTORY_CONTRACT=" + address + " " + instruction);
    }

    @Override
    public void run() throws Exception {
        // Platform factory: select an inline 0x218-byte slot for reserved keys
        // or allocate an external object, then construct and register it.
        requireInstruction(0x8234bdf0L, "cmplw", "r31,r11");
        requireInstruction(0x8234bdf8L, "bl", "0x8234cb00");
        requireInstruction(0x8234be08L, "mulli", "r11,r11,0x218");
        requireInstruction(0x8234be1cL, "bl", "0x823500c8");
        requireInstruction(0x8234be38L, "bl", "0x8234ae78");

        // Public factory publishes the newly created object through r6 and
        // takes one reference. Its caller retains the description input;
        // the platform factory itself only consumes the manager and slot.
        requireInstruction(0x8233f260L, "addi", "r3,r3,0x4");
        requireInstruction(0x8233f268L, "bl", "0x8234bdd8");
        requireInstruction(0x8233f270L, "stw", "r3,0x0(r31)");
        requireInstruction(0x8233f288L, "lwz", "r11,0x4(r11)");

        // General create path preserves r6/r7 as the two initializer inputs.
        requireInstruction(0x82343f94L, "or", "r29,r6,r6");
        requireInstruction(0x82343f98L, "or", "r27,r7,r7");
        requireInstruction(0x82343fb8L, "bl", "0x8233f250");
        requireInstruction(0x82343fd0L, "or", "r5,r27,r27");
        requireInstruction(0x82343fd4L, "or", "r4,r29,r29");
        requireInstruction(0x82343fdcL, "lwz", "r11,0x10(r11)");

        // One built-in context demonstrates the same contract with a fixed
        // description blob and reserved slot -12.
        requireInstruction(0x82341308L, "bl", "0x823440a8");
        requireInstruction(0x8234131cL, "li", "r30,-0xc");
        requireInstruction(0x82341330L, "bl", "0x8233f250");
        requireInstruction(0x8234133cL, "or", "r4,r29,r29");
        requireInstruction(0x82341344L, "lwz", "r11,0x10(r11)");

        // The built-in container exposes a payload at +0x20. Within that
        // payload, +0x08/+0x0c select two relative subrecords, whose first
        // words are then resolved relative to the same payload base.
        requireInstruction(0x823440a8L, "addi", "r3,r3,0x20");
        requireInstruction(0x823440c0L, "lwz", "r11,0x8(r3)");
        requireInstruction(0x823440d0L, "lwz", "r11,0xc(r3)");
        requireInstruction(0x82344100L, "lwz", "r11,0x0(r3)");

        println("AC6_SHADER_CONTEXT_FACTORY_ASSERTIONS=" + assertions);
    }
}
