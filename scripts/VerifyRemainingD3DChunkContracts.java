// Read-only assertions for corrected AC6 PAL D3D chunk register contracts.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyRemainingD3DChunkContracts extends GhidraScript {
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
        println("AC6_REMAINING_D3D_CONTRACT=" + address + " " + instruction);
    }

    @Override
    public void run() throws Exception {
        // 0x821D95C8 is internal state in 0x821D9588, not a render-target ABI.
        require(0x821d9594L, "or", "r30,r3,r3");
        require(0x821d95b0L, "lwz", "r11,0x5480(r30)");
        require(0x821d95c8L, "stw", "r11,0x5480(r30)");

        // 0x821D9D38 dispatches on a command selector in r3, not a device.
        require(0x821d9d30L, "cmplwi", "r3,0x22");
        require(0x821d9d34L, "bgt", "0x821d9dd0");
        require(0x821d9d38L, "beq", "0x821d9dc8");

        // The true viewport input is intact at 0x821DCF28; its four words are
        // later published and the state validator is called afterwards.
        require(0x821dcf14L, "lwz", "r11,0x0(r4)");
        require(0x821dcf1cL, "lwz", "r10,0x4(r4)");
        require(0x821dcf24L, "lwz", "r8,0x8(r4)");
        require(0x821dcf28L, "stfiwx", "f13,0,r7");
        require(0x821dcf3cL, "lwz", "r31,0xc(r4)");
        require(0x821dcf50L, "stw", "r11,0x317c(r3)");
        require(0x821dcf5cL, "stw", "r31,0x3188(r3)");
        require(0x821dcfccL, "bl", "0x821da658");

        // 0x821DA698 is inside that later validator, not a viewport entry.
        require(0x821da664L, "or", "r31,r3,r3");
        require(0x821da678L, "lwz", "r11,0x30(r31)");
        require(0x821da698L, "beq", "0x821da6a4");
        require(0x821da6acL, "lwz", "r10,0x3090(r31)");

        // Clear preserves the robust device/flags/depth values in r29/r28/f31.
        require(0x821e235cL, "fmr", "f31,f1");
        require(0x821e2364L, "or", "r29,r3,r3");
        require(0x821e2368L, "or", "r28,r6,r6");
        require(0x821e2380L, "vsldoi", "v0,v0,v0,0x4");

        // Resolve has already remapped its original ABI at 0x821E2BB8.
        require(0x821e2b8cL, "or", "r25,r4,r4");
        require(0x821e2b90L, "fmr", "f30,f1");
        require(0x821e2b94L, "or", "r20,r6,r6");
        require(0x821e2b9cL, "or", "r26,r5,r5");
        require(0x821e2ba0L, "or", "r31,r3,r3");
        require(0x821e2ba4L, "or", "r27,r7,r7");
        require(0x821e2bacL, "or", "r5,r8,r8");
        require(0x821e2bb0L, "or", "r22,r9,r9");
        require(0x821e2bb8L, "or", "r21,r10,r10");

        println("AC6_REMAINING_D3D_ASSERTIONS=" + assertions);
    }
}
