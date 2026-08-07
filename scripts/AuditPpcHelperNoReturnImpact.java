// Read-only audit of the PPC save/restore helper island and the analysis gap it
// opens. Measures, in the canonical project itself rather than in exports/:
//   - the noreturn flag on every __savegprlr_N / __restgprlr_N entry point;
//   - the flow override on every call site targeting one of those entries;
//   - the disassembly coverage of the qualified code range;
//   - whether 0x8200F5A8 carries a reference.
// Makes no change. Run with -readOnly -noanalysis.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.FlowType;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class AuditPpcHelperNoReturnImpact extends GhidraScript {

    private static final String QUALIFIED_XEX_SHA256 =
        "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";

    // Layout asserted independently by VerifyPpcAbiSaveRestoreHelpers.java.
    private static final long SAVE_FIRST = 0x82382ec0L;   // __savegprlr_14
    private static final long REST_FIRST = 0x82382f10L;   // __restgprlr_14
    private static final int  HELPER_COUNT = 18;          // r14..r31

    private static final long CODE_MIN = 0x82090000L;     // ghidra-bridge.yaml
    private static final long CODE_MAX = 0x823e7ff7L;

    private static final long SUBMISTBL_ERROR_STRING = 0x8200f5a8L;

    private int noReturnHelpers;
    private int overriddenCallSites;
    private int totalCallSites;

    private void auditHelper(long entry, String expectedName) throws Exception {
        Address address = toAddr(entry);
        Function helper = getFunctionAt(address);
        String name = helper == null ? "<no function>" : helper.getName();
        boolean noReturn = helper != null && helper.hasNoReturn();
        if (noReturn) {
            noReturnHelpers++;
        }

        int callers = 0;
        int overridden = 0;
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(address);
        while (references.hasNext()) {
            Reference reference = references.next();
            Instruction call = getInstructionAt(reference.getFromAddress());
            if (call == null) {
                continue;
            }
            callers++;
            FlowType flow = call.getFlowType();
            // A returning bl must fall through. No fall-through means the call
            // terminates the body and everything after it stays undefined.
            if (call.getFallThrough() == null || !flow.isCall()) {
                overridden++;
            }
        }
        totalCallSites += callers;
        overriddenCallSites += overridden;

        println(String.format(
            "AC6_HELPER %s expected=%s name=%s noreturn=%s callers=%d without_fallthrough=%d",
            address, expectedName, name, noReturn, callers, overridden));
    }

    private void auditCoverage() {
        Address min = toAddr(CODE_MIN);
        Address max = toAddr(CODE_MAX);
        long defined = 0;
        InstructionIterator instructions =
            currentProgram.getListing().getInstructions(min, true);
        while (instructions.hasNext()) {
            Instruction instruction = instructions.next();
            if (instruction.getAddress().compareTo(max) > 0) {
                break;
            }
            defined += instruction.getLength();
        }
        long range = max.subtract(min) + 1;
        println(String.format(
            "AC6_COVERAGE defined_bytes=%d range_bytes=%d percent=%.1f",
            defined, range, 100.0 * defined / range));

        long functionBytes = 0;
        int functionCount = 0;
        for (Function function : currentProgram.getFunctionManager()
                .getFunctions(min, true)) {
            if (function.getEntryPoint().compareTo(max) > 0) {
                break;
            }
            AddressSetView body = function.getBody();
            functionBytes += body.getNumAddresses();
            functionCount++;
        }
        println(String.format(
            "AC6_FUNCTIONS count=%d body_bytes=%d percent=%.1f",
            functionCount, functionBytes, 100.0 * functionBytes / range));
    }

    private void auditParserErrorString() {
        Address string = toAddr(SUBMISTBL_ERROR_STRING);
        int count = 0;
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(string);
        while (references.hasNext()) {
            Reference reference = references.next();
            println("AC6_SUBMISTBL_XREF from=" + reference.getFromAddress()
                + " type=" + reference.getReferenceType());
            count++;
        }
        // Predicted single materialization site, decoded from the loaded image.
        println("AC6_SUBMISTBL_XREF_COUNT " + count + " predicted_site=0x823096c8");
    }

    @Override
    protected void run() throws Exception {
        String sha = currentProgram.getExecutableSHA256();
        if (!QUALIFIED_XEX_SHA256.equalsIgnoreCase(sha)) {
            throw new IllegalStateException("unexpected XEX SHA-256: " + sha);
        }
        println("AC6_PROGRAM " + currentProgram.getName()
            + " base=" + currentProgram.getImageBase() + " sha256=" + sha);

        for (int index = 0; index < HELPER_COUNT; ++index) {
            auditHelper(SAVE_FIRST + 4L * index, "__savegprlr_" + (14 + index));
        }
        for (int index = 0; index < HELPER_COUNT; ++index) {
            auditHelper(REST_FIRST + 4L * index, "__restgprlr_" + (14 + index));
        }
        println(String.format(
            "AC6_HELPER_SUMMARY noreturn_entries=%d/%d call_sites=%d without_fallthrough=%d",
            noReturnHelpers, 2 * HELPER_COUNT, totalCallSites, overriddenCallSites));

        auditCoverage();
        auditParserErrorString();
    }
}
