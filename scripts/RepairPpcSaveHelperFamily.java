// Repair the PPC nonvolatile-register save-helper family and the analysis gap it
// opens in the canonical PAL project.
//
// Defect. The imported XEX marks part of the __savegprlr_N island as no-return.
// A prologue `mflr r12 ; bl __savegprlr_N` therefore terminates its own body:
// the call carries a CALL_TERMINATOR flow override, nothing falls through, and
// the rest of the function is never disassembled. The audit script
// AuditPpcHelperNoReturnImpact.java measures 11/36 entries still no-return and
// 3871 of 4128 save-helper call sites without a fall-through.
//
// This script clears the flag on every save entry, clears the stale per-call
// override that survives the flag, seeds disassembly at each restored
// fall-through, and recomputes the body of every affected function. It does not
// create new functions; unreached code found this way is left for the separate
// function-start pass.
//
// The restore island is intentionally untouched: `b __restgprlr_N` is a tail
// branch and correctly has no fall-through.
//
// Byte-qualified against the loaded image before any change. Run WITHOUT
// -readOnly. Idempotent.
// @category AC6

import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.FlowOverride;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.SourceType;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

public class RepairPpcSaveHelperFamily extends GhidraScript {

    private static final String QUALIFIED_XEX_SHA256 =
        "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";

    // Layout asserted independently by VerifyPpcAbiSaveRestoreHelpers.java and
    // decoded from analysis-input/ACE6_X360.exe.
    private static final long SAVE_FIRST = 0x82382ec0L;   // __savegprlr_14
    private static final long SAVE_LAST_INSTR = 0x82382f0cL;   // trailing blr
    private static final long REST_FIRST = 0x82382f10L;   // __restgprlr_14
    private static final long REST_LAST_INSTR = 0x82382f60L;   // trailing blr
    private static final int  HELPER_COUNT = 18;          // r14..r31

    private static final String SAVE_CHAIN_SHA256 =
        "c91d6acdcca047ceaa15c988eab916f741a0dcb81941ab90e45888ef845539e0";
    private static final String REST_CHAIN_SHA256 =
        "f5f9f5c1203f5d2d1a3e554b1a00333f1a7e0fe6245eeea147424b7914efe443";

    private void qualifyRange(long startValue, long lastInstructionValue,
            String expectedSha256) throws Exception {
        Address start = toAddr(startValue);
        long length = lastInstructionValue - startValue + 4;
        byte[] bytes = new byte[(int) length];
        // Read byte by byte: bulk reads report undefined instructions as zero in
        // this imported XEX, which would defeat the guard exactly where it
        // matters most.
        for (int index = 0; index < bytes.length; ++index) {
            bytes[index] = currentProgram.getMemory().getByte(start.add(index));
        }
        byte[] digest = MessageDigest.getInstance("SHA-256").digest(bytes);
        StringBuilder actual = new StringBuilder(64);
        for (byte value : digest) {
            actual.append(String.format("%02x", value & 0xff));
        }
        if (!expectedSha256.equalsIgnoreCase(actual.toString())) {
            throw new IllegalStateException("helper chain SHA-256 mismatch at " + start
                + ": expected " + expectedSha256 + ", got " + actual);
        }
        println("AC6_QUALIFIED_CHAIN " + start + " bytes=" + length + " sha256=" + actual);
    }

    /** Clears no-return on one save entry, creating the function if absent. */
    private void repairSaveEntry(long entryValue, String name) throws Exception {
        Address entry = toAddr(entryValue);
        Function helper = getFunctionAt(entry);
        if (helper == null) {
            if (getInstructionAt(entry) == null) {
                Disassembler.getDisassembler(currentProgram, monitor, null)
                    .disassemble(entry, null);
            }
            helper = createFunction(entry, name);
        }
        if (helper == null) {
            throw new IllegalStateException("could not create save helper at " + entry);
        }
        helper.setNoReturn(false);
        helper.setName(name, SourceType.USER_DEFINED);
        println("AC6_SAVE_HELPER " + entry + " " + helper.getName()
            + " noreturn=" + helper.hasNoReturn());
    }

    /**
     * Labels one restore entry without creating a function. The island is a
     * single fall-through chain; splitting it into 18 functions would destroy
     * the tail-branch semantics every caller relies on.
     */
    private void labelRestoreEntry(long entryValue, String name) throws Exception {
        Address entry = toAddr(entryValue);
        Function existing = getFunctionAt(entry);
        if (existing != null) {
            existing.setName(name, SourceType.USER_DEFINED);
            println("AC6_REST_HELPER " + entry + " " + existing.getName() + " (function)");
            return;
        }
        createLabel(entry, name, true, SourceType.USER_DEFINED);
        println("AC6_REST_HELPER " + entry + " " + name + " (label)");
    }

    @Override
    protected void run() throws Exception {
        String sha = currentProgram.getExecutableSHA256();
        if (!QUALIFIED_XEX_SHA256.equalsIgnoreCase(sha)) {
            throw new IllegalStateException("unexpected XEX SHA-256: " + sha);
        }
        qualifyRange(SAVE_FIRST, SAVE_LAST_INSTR, SAVE_CHAIN_SHA256);
        qualifyRange(REST_FIRST, REST_LAST_INSTR, REST_CHAIN_SHA256);

        // Phase 1 — the island itself.
        for (int index = 0; index < HELPER_COUNT; ++index) {
            repairSaveEntry(SAVE_FIRST + 4L * index, "__savegprlr_" + (14 + index));
        }
        for (int index = 0; index < HELPER_COUNT; ++index) {
            labelRestoreEntry(REST_FIRST + 4L * index, "__restgprlr_" + (14 + index));
        }

        // Phase 2 — the call sites. Clearing the flag does not retroactively
        // clear the override already stored on each instruction.
        List<Address> restoredFallThroughs = new ArrayList<>();
        Set<Function> affected = new LinkedHashSet<>();
        int inspected = 0;
        int cleared = 0;
        for (int index = 0; index < HELPER_COUNT; ++index) {
            Address helper = toAddr(SAVE_FIRST + 4L * index);
            ReferenceIterator references =
                currentProgram.getReferenceManager().getReferencesTo(helper);
            while (references.hasNext()) {
                Reference reference = references.next();
                Instruction call = getInstructionAt(reference.getFromAddress());
                if (call == null || !call.getFlowType().isCall()) {
                    continue;
                }
                inspected++;
                if (call.getFallThrough() != null) {
                    continue;
                }
                call.setFlowOverride(FlowOverride.NONE);
                if (call.getFallThrough() == null) {
                    println("AC6_CALL_STILL_TERMINAL " + call.getAddress());
                    continue;
                }
                cleared++;
                restoredFallThroughs.add(call.getFallThrough());
                Function owner = getFunctionContaining(call.getAddress());
                if (owner != null) {
                    affected.add(owner);
                }
            }
        }
        println("AC6_CALL_SITES inspected=" + inspected + " cleared=" + cleared);

        // Phase 3 — seed disassembly at every restored fall-through.
        Disassembler disassembler =
            Disassembler.getDisassembler(currentProgram, monitor, null);
        int seeded = 0;
        for (Address fallThrough : restoredFallThroughs) {
            if (getInstructionAt(fallThrough) == null) {
                disassembler.disassemble(fallThrough, null);
                seeded++;
            }
        }
        println("AC6_DISASSEMBLY_SEEDS " + seeded + " of " + restoredFallThroughs.size());

        // Phase 4 — recompute the body of every function that was truncated.
        int fixed = 0;
        int failed = 0;
        for (Function function : affected) {
            long before = function.getBody().getNumAddresses();
            if (CreateFunctionCmd.fixupFunctionBody(currentProgram, function, monitor)) {
                long after = function.getBody().getNumAddresses();
                if (after > before) {
                    fixed++;
                }
            } else {
                failed++;
                println("AC6_BODY_FIXUP_FAILED " + function.getEntryPoint());
            }
        }
        println("AC6_BODY_FIXUP affected=" + affected.size()
            + " grown=" + fixed + " failed=" + failed);
    }
}
