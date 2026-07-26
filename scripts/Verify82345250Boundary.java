// Read-only assertions for the AC6 PAL 0x82345250 configured split.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify82345250Boundary extends GhidraScript {
    private int assertions;
    private void require(long value, String mnemonic, String fragment) throws Exception {
        Address a = toAddr(value); Instruction i = currentProgram.getListing().getInstructionAt(a);
        if (i == null) {
            Disassembler.getDisassembler(currentProgram, monitor, null).disassemble(a, null);
            i = currentProgram.getListing().getInstructionAt(a);
        }
        if (i == null || !mnemonic.equals(i.getMnemonicString()) || !i.toString().contains(fragment))
            throw new IllegalStateException(a + " unexpected " + i);
        assertions++;
    }
    private void noRefs(long value) {
        ReferenceIterator it = currentProgram.getReferenceManager().getReferencesTo(toAddr(value));
        if (it.hasNext()) throw new IllegalStateException(toAddr(value) + " has reference " + it.next());
        assertions++;
    }
    private void noFunction(long value) {
        if (currentProgram.getFunctionManager().getFunctionAt(toAddr(value)) != null)
            throw new IllegalStateException(toAddr(value) + " unexpectedly starts function");
        assertions++;
    }
    @Override public void run() throws Exception {
        require(0x82345100L, "mfspr", "r12,LR");
        require(0x82345108L, "stwu", "r1,-0x120(r1)");
        require(0x82345230L, "addi", "r31,r25,0x486");
        require(0x82345234L, "li", "r26,0x3");
        require(0x8234523cL, "or", "r29,r11,r11");
        require(0x82345240L, "li", "r27,0x6");
        require(0x82345244L, "addi", "r30,r22,0x4");
        require(0x82345248L, "li", "r28,0x8");
        require(0x8234524cL, "lhz", "r9,-0x2(r30)");
        require(0x82345250L, "addi", "r11,r1,0x50");
        noRefs(0x82345250L);
        require(0x82345254L, "cmplwi", "r9,0x0");
        require(0x82345258L, "beq", "0x82345288");
        require(0x82345260L, "lwz", "r8,0x0(r10)");
        require(0x82345284L, "bne", "0x82345260");
        require(0x823452f8L, "bl", "0x821de898");
        require(0x823452fcL, "stw", "r3,-0x6(r31)");
        require(0x82345300L, "subic.", "r28,r28,0x1");
        require(0x82345304L, "addi", "r30,r30,0x8");
        require(0x82345308L, "addi", "r31,r31,0x8");
        require(0x8234530cL, "bne", "0x8234524c");
        require(0x82345318L, "bne", "0x82345244");
        require(0x82345324L, "bne", "0x8234523c");
        require(0x82345328L, "addi", "r1,r1,0x120");
        noFunction(0x82345250L); noFunction(0x8234524cL); noFunction(0x8234530cL);
        Function owner = currentProgram.getFunctionManager().getFunctionAt(toAddr(0x82345100L));
        if (owner == null) throw new IllegalStateException("missing sole ABI entry");
        assertions++;
        println("AC6_82345250_ASSERTIONS=" + assertions);
    }
}
