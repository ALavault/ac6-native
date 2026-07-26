// Read-only assertions for the AC6 PAL vertex-declaration device binding.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyVertexDeclarationBindContracts extends GhidraScript {
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
        println("AC6_VERTEX_DECL_BIND_CONTRACT=" + address + " " + instruction);
    }

    @Override
    public void run() throws Exception {
        // Physical setter: r3=device, r4=declaration. The store and dirty bit
        // are both unconditional.
        require(0x821de790L, "stw", "r4,0x2e24(r3)");
        require(0x821de794L, "ld", "r11,0x10(r3)");
        require(0x821de798L, "oris", "r11,r11,0x8");
        require(0x821de79cL, "std", "r11,0x10(r3)");
        require(0x821de7a0L, "blr", "blr");

        // Representative direct callers load a declaration table entry into
        // r4, bind it, then configure one or more vertex streams.
        require(0x82345420L, "lwz", "r4,0x50(r1)");
        require(0x82345424L, "bl", "0x821de790");
        require(0x82345440L, "bl", "0x821dd068");
        require(0x823454acL, "lwz", "r4,0x50(r1)");
        require(0x823454b0L, "bl", "0x821de790");
        require(0x823454ccL, "bl", "0x821dd068");
        require(0x823454e8L, "bl", "0x821dd068");
        require(0x82345558L, "lwz", "r4,0x50(r1)");
        require(0x8234555cL, "bl", "0x821de790");
        require(0x823455f8L, "lwz", "r4,0x50(r1)");
        require(0x823455fcL, "bl", "0x821de790");
        require(0x82345618L, "bl", "0x821dd068");
        require(0x82347124L, "lwz", "r4,0x50(r1)");
        require(0x82347128L, "bl", "0x821de790");
        require(0x82347194L, "lwz", "r4,0x50(r1)");
        require(0x82347198L, "bl", "0x821de790");

        // AC6 also inlines the exact same committed store/dirty sequence
        // immediately before draws, so a hook on the physical setter alone is
        // not a complete capture boundary.
        require(0x82138c40L, "stw", "r11,0x2e24(r31)");
        require(0x82138c44L, "ld", "r11,0x10(r31)");
        require(0x82138c48L, "oris", "r11,r11,0x8");
        require(0x82138c4cL, "std", "r11,0x10(r31)");
        require(0x82138c6cL, "bl", "0x821deed8");

        // Device teardown and declaration-owner release both inline an
        // explicit null bind followed by the same dirty bit.
        require(0x821e6e44L, "li", "r11,0x0");
        require(0x821e6e50L, "stw", "r11,0x2e24(r31)");
        require(0x821e6e58L, "oris", "r11,r11,0x8");
        require(0x821d8fa4L, "li", "r29,0x0");
        require(0x821d8fb4L, "stw", "r29,0x2e24(r31)");
        require(0x821d8fbcL, "oris", "r11,r11,0x8");
        require(0x821d8fc8L, "bl", "0x821e3ff8");

        // The common draw-state compiler consumes the same device field.
        // Its configured 0x821ED210 chunk is conditional; the load at 0x218
        // is the actual common observation point.
        require(0x821ed208L, "beq", "0x821ed214");
        require(0x821ed210L, "or", "r20,r3,r3");
        require(0x821ed218L, "lwz", "r21,0x2e24(r30)");

        // The declaration object itself is produced from a terminated element
        // sequence and stored into an owner field, distinct from the device.
        require(0x821d8e7cL, "bl", "0x821de898");
        require(0x821d8e90L, "stw", "r11,0x15c(r31)");

        println("AC6_VERTEX_DECL_BIND_ASSERTIONS=" + assertions);
    }
}
