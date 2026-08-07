// Create functions at direct call targets the linker never recorded.
//
// .pdata only lists functions that need unwind information. Leaf functions that
// touch no nonvolatile register and allocate no frame are absent from it, so the
// .pdata pass cannot create them, and they surface in the decompiler as bare
// `func_0x82330f98(...)` calls with no body. Two of ObjBin::read's three
// sub-record parsers are exactly this case.
//
// A direct `bl` is unambiguous evidence of a function start: the target is an
// address the program calls and returns from. This pass creates a function at
// every such target inside the qualified code range that has none, and lets
// Ghidra derive the body from flow.
//
// It deliberately ignores `bctrl` and other indirect calls, which carry no
// static target, and never touches an address that already has a function or
// that falls inside another function's body.
//
// Run WITHOUT -readOnly. Idempotent.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import java.util.LinkedHashSet;
import java.util.Set;

public class CreateLeafCallTargetFunctions extends GhidraScript {

    private static final String QUALIFIED_XEX_SHA256 =
        "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";

    private static final long CODE_MIN = 0x82090000L;
    private static final long CODE_MAX = 0x823e7ff7L;

    @Override
    protected void run() throws Exception {
        String sha = currentProgram.getExecutableSHA256();
        if (!QUALIFIED_XEX_SHA256.equalsIgnoreCase(sha)) {
            throw new IllegalStateException("unexpected XEX SHA-256: " + sha);
        }

        Address min = toAddr(CODE_MIN);
        Address max = toAddr(CODE_MAX);

        Set<Address> targets = new LinkedHashSet<>();
        InstructionIterator instructions =
            currentProgram.getListing().getInstructions(min, true);
        while (instructions.hasNext()) {
            if (monitor.isCancelled()) {
                break;
            }
            Instruction instruction = instructions.next();
            if (instruction.getAddress().compareTo(max) > 0) {
                break;
            }
            if (!instruction.getFlowType().isCall()) {
                continue;
            }
            for (Address flow : instruction.getFlows()) {
                if (flow.compareTo(min) >= 0 && flow.compareTo(max) <= 0) {
                    targets.add(flow);
                }
            }
        }
        println("AC6_CALL_TARGETS distinct=" + targets.size());

        Disassembler disassembler =
            Disassembler.getDisassembler(currentProgram, monitor, null);

        int present = 0;
        int inBody = 0;
        int created = 0;
        int failed = 0;
        for (Address target : targets) {
            if (monitor.isCancelled()) {
                break;
            }
            if (getFunctionAt(target) != null) {
                present++;
                continue;
            }
            // Do not carve a new function out of an existing body: a call into
            // the middle of a function is a boundary question of its own.
            Function containing = getFunctionContaining(target);
            if (containing != null) {
                inBody++;
                println("AC6_CALL_TARGET_INSIDE " + target
                    + " within " + containing.getEntryPoint());
                continue;
            }
            if (getInstructionAt(target) == null) {
                disassembler.disassemble(target, null);
            }
            if (getInstructionAt(target) == null) {
                failed++;
                continue;
            }
            if (createFunction(target, null) == null) {
                failed++;
                println("AC6_CALL_TARGET_CREATE_FAILED " + target);
                continue;
            }
            created++;
        }

        println(String.format(
            "AC6_LEAF_FUNCTIONS already_present=%d inside_existing_body=%d created=%d failed=%d",
            present, inBody, created, failed));
    }
}
