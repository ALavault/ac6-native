// Read-only assertions for the AC6 PAL texture-context sampler-state boundary.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyTextureSamplerState extends GhidraScript {
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
        println("AC6_SAMPLER_STATE=" + address + " " + instruction);
    }

    private void requireU32(long value, long expected) throws Exception {
        Address address = toAddr(value);
        long actual = Integer.toUnsignedLong(currentProgram.getMemory().getInt(address, true));
        if (actual != expected) {
            throw new IllegalStateException(address + " expected 0x" +
                Long.toHexString(expected) + " but found 0x" + Long.toHexString(actual));
        }
        println("AC6_SAMPLER_TABLE=" + address + " 0x" + Long.toHexString(actual));
    }

    @Override
    public void run() throws Exception {
        // Texture-context byte slot +0x2c binds the descriptor, then configures
        // the two filter paths. The mip implementation adds the anisotropy path.
        require(0x8234efe4L, "bl", "821e1088");
        require(0x8234f018L, "bl", "821dc4f8");
        require(0x8234f028L, "bl", "821dc688");
        require(0x8234fe2cL, "bl", "821e1088");
        require(0x8234fe5cL, "bl", "821dc4f8");
        require(0x8234fe6cL, "bl", "821dc688");
        require(0x8234fe88L, "bl", "821dc908");

        // Both filter setters preserve device+sampler in r8 before r4 becomes
        // a table byte offset at the replaceable instrumentation chunks.
        require(0x821dc514L, "add", "r8,r3,r4");
        require(0x821dc52cL, "rlwinm", "r4,r4,0x2");
        require(0x821dc538L, "lwzx", "r4,r4,r7");
        require(0x821dc568L, "rlwimi", "r31,r10,0x15,0x4,0x6");
        require(0x821dc574L, "stw", "r31,0xc(r11)");
        require(0x821dc6a4L, "add", "r8,r3,r4");
        require(0x821dc6bcL, "rlwinm", "r4,r4,0x2");
        require(0x821dc6c8L, "lwzx", "r4,r4,r7");
        require(0x821dc6f4L, "rlwimi", "r31,r10,0x13,0xb,0xc");
        require(0x821dc6f8L, "rlwimi", "r31,r10,0x13,0x4,0x6");
        require(0x821dc704L, "stw", "r31,0xc(r11)");

        // The optional mip path translates the stored anisotropy enum and
        // inserts the resulting three-bit Xenos aniso_filter field.
        require(0x821dc948L, "lwzx", "r10,r7,r10");
        require(0x821dc94cL, "rlwimi", "r9,r10,0x19,0x4,0x6");
        require(0x821dc950L, "stw", "r9,0xc(r11)");
        require(0x821dc968L, "stb", "r5,0x2e8c(r10)");

        // Beginning of the enum-to-Xenos conversion table. These values are
        // structural evidence only; no unsupported D3D enum name is inferred.
        requireU32(0x82067fa0L, 0L);
        requireU32(0x82067fa4L, 0L);
        requireU32(0x82067fa8L, 2L);
        requireU32(0x82067facL, 2L);
        requireU32(0x82067fb0L, 3L);
        requireU32(0x82067fbcL, 4L);
        requireU32(0x82067fd4L, 5L);
    }
}
