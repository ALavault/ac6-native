// Read-only assertions for AC6 PAL configured sampler chunks and live values.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class VerifySamplerChunkContracts extends GhidraScript {
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
        println("AC6_SAMPLER_CHUNK=" + address + " " + instruction);
    }

    private void requireContainingFunction(long value, long expectedEntry)
            throws Exception {
        Address address = toAddr(value);
        Function function = currentProgram.getFunctionManager().getFunctionContaining(address);
        Address expected = toAddr(expectedEntry);
        if (function == null || !expected.equals(function.getEntryPoint())) {
            throw new IllegalStateException(address + " expected containing function " +
                expected + " but found " +
                (function == null ? "<none>" : function.getEntryPoint()));
        }
        println("AC6_SAMPLER_FUNCTION=" + address + " entry=" + function.getEntryPoint());
    }

    @Override
    public void run() throws Exception {
        requireContainingFunction(0x821dc9c0L, 0x821dc980L);
        requireContainingFunction(0x821dca68L, 0x821dca28L);
        requireContainingFunction(0x821dcb08L, 0x821dcac8L);
        requireContainingFunction(0x821dcb88L, 0x821dcb48L);

        // Raw float input r5 is converted before the aniso-bias chunk. r4 is
        // still the sampler; the chunk writes dword_5 bits 5..8.
        require(0x821dc980L, "stw", "r5,0x24(r1)");
        require(0x821dc9a4L, "fmuls", "f0,f13,f0");
        require(0x821dc9c0L, "lwz", "r9,0x14(r11)");
        require(0x821dc9c8L, "rlwimi", "r9,r8,0x5,0x17,0x1a");
        require(0x821dc9ccL, "stw", "r9,0x14(r11)");

        // Raw float input r5 is converted before the LOD-bias chunk. The
        // chunk writes dword_4 bits 12..21.
        require(0x821dca28L, "stw", "r5,0x24(r1)");
        require(0x821dca4cL, "fmuls", "f0,f13,f0");
        require(0x821dca68L, "lwz", "r9,0x10(r11)");
        require(0x821dca70L, "rlwimi", "r9,r8,0xc,0xa,0x13");
        require(0x821dca74L, "stw", "r9,0x10(r11)");

        // r11 is the effective clamped minimum mip level when the configured
        // chunk is entered; it is written to dword_4 bits 2..5.
        require(0x821dcaf0L, "cmplw", "r11,r5");
        require(0x821dcaf8L, "or", "r11,r5,r5");
        require(0x821dcb08L, "rldicl", "r9,r9,0x0,0x20");
        require(0x821dcb10L, "rlwimi", "r8,r11,0x2,0x1a,0x1d");
        require(0x821dcb14L, "stw", "r8,0x10(r10)");

        // r11 is the effective clamped maximum mip level at this chunk; it is
        // written to dword_4 bits 6..9.
        require(0x821dcb70L, "cmplw", "r11,r5");
        require(0x821dcb78L, "or", "r11,r5,r5");
        require(0x821dcb88L, "rldicl", "r9,r9,0x0,0x20");
        require(0x821dcb90L, "rlwimi", "r8,r11,0x6,0x16,0x19");
        require(0x821dcb94L, "stw", "r8,0x10(r10)");
    }
}
