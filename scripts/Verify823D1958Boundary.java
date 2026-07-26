// Read-only assertions for the AC6 PAL 0x823D1938 nested-loop boundary.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify823D1958Boundary extends GhidraScript {
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
        println("AC6_823D1958_CONTRACT=" + address + " " + instruction);
    }

    private void requireNoReferences(long value) {
        Address address = toAddr(value);
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(address);
        if (references.hasNext()) {
            throw new IllegalStateException(address + " has an incoming reference: " +
                references.next());
        }
        assertions++;
        println("AC6_823D1958_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The real entry derives the table base and outer-loop cursor.
        require(0x823d1938L, "lis", "r11,-0x7d97");
        require(0x823d193cL, "li", "r9,0x0");
        require(0x823d1940L, "addi", "r8,r11,0x5d08");
        require(0x823d1944L, "addi", "r11,r8,0x98");

        // The outer and inner heads initialize and consume loop state.
        require(0x823d1948L, "li", "r10,0x3");
        require(0x823d194cL, "subi", "r10,r10,0x1");
        require(0x823d1950L, "std", "r9,-0x90(r11)");
        require(0x823d1954L, "std", "r9,0x0(r11)");

        // The configured split is a mid-body cursor update. It needs r11 and
        // r10 prepared above and has no independent incoming reference.
        require(0x823d1958L, "addi", "r11,r11,0x8");
        requireNoReferences(0x823d1958L);
        require(0x823d195cL, "cmplwi", "cr6,r10,0x0");
        require(0x823d1960L, "bne", "cr6,0x823d194c");

        // The outer loop also returns to state established before the split.
        require(0x823d1964L, "addi", "r10,r8,0x128");
        require(0x823d1968L, "cmpw", "cr6,r11,r10");
        require(0x823d196cL, "blt", "cr6,0x823d1948");

        // The function tail dispatches after both loops; the next independent
        // thunk begins at 0x823D1980.
        require(0x823d1970L, "lis", "r11,-0x7dc3");
        require(0x823d1974L, "addi", "r3,r11,0x4dd0");
        require(0x823d1978L, "b", "0x82380040");
        require(0x823d1980L, "lis", "r11,-0x7dc3");
        require(0x823d1984L, "addi", "r3,r11,0x4de8");

        println("AC6_823D1958_ASSERTIONS=" + assertions);
    }
}
