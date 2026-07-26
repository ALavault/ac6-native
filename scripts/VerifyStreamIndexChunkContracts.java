// Read-only assertions for the AC6 PAL render-target and depth-stencil chunks.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyStreamIndexChunkContracts extends GhidraScript {
    private int assertions;

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
        assertions++;
        println("AC6_RT_DEPTH_CONTRACT=" + address + " " + instruction);
    }

    @Override
    public void run() throws Exception {
        // 0x821DD068 is the distinct six-argument stream-source setter. It
        // preserves device/slot/buffer/flags in r31/r29/r30/r26, uses the
        // 0x30A4 + 4*slot table, and stores the buffer there. This prevents the
        // 0x821DD220 color-target setter from being mislabeled as a stream bind.
        require(0x821dd068L, "mfspr", "r12,LR");
        require(0x821dd074L, "or", "r30,r5,r5");
        require(0x821dd078L, "or", "r31,r3,r3");
        require(0x821dd07cL, "or", "r29,r4,r4");
        require(0x821dd080L, "or", "r26,r7,r7");
        require(0x821dd088L, "beq", "0x821dd0d8");
        require(0x821dd094L, "lwz", "r9,0x1c(r30)");
        require(0x821dd098L, "add", "r10,r10,r6");
        require(0x821dd09cL, "subf", "r9,r6,r9");
        require(0x821dd0a8L, "addi", "r10,r6,0x200");
        require(0x821dd0c4L, "stw", "r9,0x6f4(r11)");
        require(0x821dd0c8L, "stwx", "r10,r6,r31");
        require(0x821dd0d8L, "addi", "r11,r29,0xc29");
        require(0x821dd150L, "rlwinm.", "r11,r26,0x1e,0x2,0x1f");
        require(0x821dd154L, "stwx", "r30,r27,r31");
        require(0x821dd15cL, "stb", "r11,0x30e8(r10)");

        // 0x821DD220(device, target, surface): the function stores the surface
        // in device + 0x3090 + 4*target. At the configured 0x821DD258 chunk,
        // r3/r4/r5 still hold device/target/surface before r3 is repurposed.
        require(0x821dd220L, "mfspr", "r12,LR");
        require(0x821dd22cL, "addi", "r11,r4,0xc24");
        require(0x821dd234L, "rlwinm", "r6,r11,0x2");
        require(0x821dd23cL, "stwx", "r5,r6,r31");
        require(0x821dd258L, "lwz", "r3,0x1c(r5)");

        // The reset path clears exactly four color render-target slots, 0..3.
        require(0x821e6dd8L, "li", "r30,0x0");
        require(0x821e6ddcL, "addi", "r29,r31,0x3090");
        require(0x821e6df4L, "or", "r4,r30,r30");
        require(0x821e6dfcL, "bl", "0x821dd220");
        require(0x821e6e08L, "cmplwi", "r30,0x4");

        // 0x821DD588(device, surface): the depth surface is the separate 0x30A0
        // binding. The configured chunk sees preserved device/resource in
        // r31/r30 rather than the already repurposed argument registers.
        require(0x821dd588L, "mfspr", "r12,LR");
        require(0x821dd59cL, "or", "r30,r4,r4");
        require(0x821dd5a0L, "or", "r31,r3,r3");
        require(0x821dd5a8L, "stw", "r30,0x30a0(r31)");
        require(0x821dd5c0L, "lwz", "r11,0x1c(r30)");
        require(0x821dd5c8L, "lbz", "r10,0x2abe(r31)");

        // Reset code treats 0x30A0 separately from the four color targets.
        require(0x821e6e10L, "lwz", "r11,0x30a0(r31)");
        require(0x821e6e20L, "li", "r4,0x0");
        require(0x821e6e28L, "bl", "0x821dd588");

        println("AC6_RT_DEPTH_ASSERTIONS=" + assertions);
    }
}
