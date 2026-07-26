// Read-only assertion for the PPC nonvolatile-register helper island.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyPpcAbiSaveRestoreHelpers extends GhidraScript {
    private void requireMnemonic(long value, String expected) throws Exception {
        Address address = toAddr(value);
        Instruction instruction = currentProgram.getListing().getInstructionAt(address);
        if (instruction == null || !expected.equals(instruction.getMnemonicString())) {
            throw new IllegalStateException(address + " expected " + expected +
                " but found " + (instruction == null ? "<none>" : instruction));
        }
        println("AC6_ABI_HELPER=" + address + " " + instruction);
    }

    @Override
    public void run() throws Exception {
        for (long address = 0x82382ec0L; address <= 0x82382ef8L; address += 4) {
            requireMnemonic(address, "std");
        }
        requireMnemonic(0x82382efcL, "std");
        requireMnemonic(0x82382f00L, "std");
        requireMnemonic(0x82382f04L, "std");
        requireMnemonic(0x82382f08L, "stw");
        requireMnemonic(0x82382f0cL, "blr");

        for (long address = 0x82382f10L; address <= 0x82382f54L; address += 4) {
            requireMnemonic(address, "ld");
        }
        requireMnemonic(0x82382f58L, "lwz");
        requireMnemonic(0x82382f5cL, "mtspr");
        requireMnemonic(0x82382f60L, "blr");
    }
}
