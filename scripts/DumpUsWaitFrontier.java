// Read-only dump of the US retail functions/instructions around the
// addresses named as the post-IB parking loop in
// reports/ac6-retail-native-codegen-gate2-r11-20260831.md (r51):
// sub_821E6AC8, sub_821E61A8, sub_821E64A8. Establishes, from the actual
// US disassembly, whether each address is a real function entry or an
// internal instruction of a containing function, then disassembles the
// containing function's full body. No assumption is carried over from any
// other target (PAL cycle 295 named a same-looking address in a different
// build and is not evidence here).
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.ReferenceIterator;

public class DumpUsWaitFrontier extends GhidraScript {
    private void dumpAddress(long value) throws Exception {
        Address address = toAddr(value);
        println("=== TARGET " + address + " ===");

        Function direct = currentProgram.getFunctionManager().getFunctionAt(address);
        if (direct != null) {
            println("FUNCTION_ENTRY=" + address);
        } else {
            println("NOT_A_FUNCTION_ENTRY=" + address);
        }

        Function containing = currentProgram.getFunctionManager().getFunctionContaining(address);
        if (containing == null) {
            println("NO_CONTAINING_FUNCTION=" + address);
            // Try a bounded disassembly window anyway, read-only best effort.
            return;
        }
        println("CONTAINING_FUNCTION=" + containing.getEntryPoint() + " name=" +
            containing.getName() + " body=" + containing.getBody());

        ReferenceIterator refsToTarget = currentProgram.getReferenceManager()
            .getReferencesTo(address);
        int refCount = 0;
        StringBuilder refDump = new StringBuilder();
        while (refsToTarget.hasNext() && refCount < 10) {
            refDump.append(refsToTarget.next().getFromAddress()).append(" ");
            refCount++;
        }
        println("REFERENCES_TO_TARGET=" + refCount + (refCount > 0 ? " [" + refDump + "]" : ""));

        AddressSetView body = containing.getBody();
        InstructionIterator instructions = currentProgram.getListing().getInstructions(body, true);
        int count = 0;
        int cap = 400;
        while (instructions.hasNext() && count < cap) {
            Instruction insn = instructions.next();
            println("INSN " + insn.getAddress() + " " + insn.toString());
            count++;
        }
        println("INSTRUCTION_COUNT=" + count + (count >= cap ? " (CAPPED)" : " (COMPLETE)"));
        println("=== END " + address + " ===");
    }

    @Override
    public void run() throws Exception {
        dumpAddress(0x821e6ac8L);
        dumpAddress(0x821e61a8L);
        dumpAddress(0x821e64a8L);
        // Also the qualified Vd fields these are believed to poll, per
        // reports/ac6-retail-native-codegen-gate2-r11-20260831.md: WPTR at
        // object+10952 and readback at state+60 are runtime addresses, not
        // static ones, so nothing static to dump for them here; listed for
        // cross-reference only.
    }
}
