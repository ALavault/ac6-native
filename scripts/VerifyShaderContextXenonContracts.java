// Read-only assertions for the AC6 PAL NU::Shader::ShaderContextXenon object.
// This qualifies the paired shader owner without calling it a MATE object.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyShaderContextXenonContracts extends GhidraScript {
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
        println("AC6_SHADER_CONTEXT_XENON_CONTRACT=" + address + " " + instruction);
    }

    private void requireWord(long addressValue, long expected) throws Exception {
        Address address = toAddr(addressValue);
        long actual = currentProgram.getMemory().getInt(address) & 0xffffffffL;
        if (actual != expected) {
            throw new IllegalStateException(String.format(
                "%s expected 0x%08x but found 0x%08x", address, expected, actual));
        }
        assertions++;
        println(String.format("AC6_SHADER_CONTEXT_XENON_WORD=%s 0x%08x", address, actual));
    }

    private void requireAscii(long addressValue, String expected) throws Exception {
        byte[] actual = new byte[expected.length()];
        currentProgram.getMemory().getBytes(toAddr(addressValue), actual);
        String value = new String(actual, java.nio.charset.StandardCharsets.US_ASCII);
        if (!expected.equals(value)) {
            throw new IllegalStateException(toAddr(addressValue) + " expected " + expected +
                " but found " + value);
        }
        assertions++;
        println("AC6_SHADER_CONTEXT_XENON_RTTI=" + expected);
    }

    @Override
    public void run() throws Exception {
        // CompleteObjectLocator -> TypeDescriptor -> decorated class name,
        // followed by the vtable whose +0x28 and +0x40 slots are the bind paths.
        requireWord(0x820126c8L, 0x8206b2a4L);
        requireWord(0x8206b2b0L, 0x8267858cL);
        requireAscii(0x82678594L, ".?AVShaderContextXenon@Shader@NU@@");
        requireWord(0x820126f4L, 0x82350318L);
        requireWord(0x8201270cL, 0x82350368L);

        // Constructor publishes this vtable and clears the paired shader
        // objects plus their retained source blocks.
        requireInstruction(0x823500f0L, "stw", "r10,0x0(r31)");
        requireInstruction(0x823500f4L, "stw", "r11,0x18(r31)");
        requireInstruction(0x823500f8L, "stw", "r11,0x1c(r31)");
        requireInstruction(0x823500fcL, "stw", "r11,0x20(r31)");
        requireInstruction(0x82350100L, "stw", "r11,0x24(r31)");

        // Initializer slot +0x10 resolves two source records and constructs
        // the vertex/pixel objects retained at +0x18/+0x1c.
        requireWord(0x820126dcL, 0x82350118L);
        requireInstruction(0x82350130L, "bl", "0x823440c0");
        requireInstruction(0x8235013cL, "bl", "0x823440d0");
        requireInstruction(0x8235016cL, "bl", "0x821de208");
        requireInstruction(0x82350190L, "stw", "r3,0x18(r31)");
        requireInstruction(0x82350194L, "bl", "0x821de488");
        requireInstruction(0x82350204L, "stw", "r3,0x1c(r31)");

        // Bind slot +0x28 publishes that exact pair to the device, then calls
        // the base-context operation. This is a shader-context boundary, not
        // evidence of a MATE identity.
        requireInstruction(0x82350334L, "lwz", "r4,0x18(r31)");
        requireInstruction(0x82350340L, "bl", "0x821de5c0");
        requireInstruction(0x82350348L, "lwz", "r4,0x1c(r31)");
        requireInstruction(0x8235034cL, "bl", "0x821de2c8");
        requireInstruction(0x82350358L, "bl", "0x8234b8e8");

        println("AC6_SHADER_CONTEXT_XENON_ASSERTIONS=" + assertions);
    }
}
