// Read-only qualification of the NTSC-U/J gameplay/world-submission owners.
// PAL addresses are not consumed here. Candidates were selected by literal
// generated-control-flow cross-match and must independently exist in the
// canonical US project with linker-owned .pdata extents.
// @category AC6.Evidence

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import java.util.Map;
import java.util.TreeMap;

public class QualifyUsWorldSubmissionOwners extends GhidraScript {
    private static final String PROJECT = "ac6-us";
    private static final String PROGRAM = "default.xex";
    private static final String LANGUAGE = "PowerPC:BE:64:Xenon";
    private static final String XEX_SHA256 =
        "6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc";
    private static final long PDATA_START = 0x82079e00L;
    private static final int PDATA_BYTES = 0xff18;
    private static final long[] CANDIDATES = {
        0x8219a510L, // CModeTaskGame event dispatcher candidate
        0x8226cea0L, // gameplay manager tick/world-submission owner candidate
        0x822704a0L, // gameplay object update/census owner candidate
        0x822638b0L, // camera update owner candidate
        0x82271908L, // radio/update sibling candidate
    };

    private static long beU32(byte[] bytes, int offset) {
        return ((long) (bytes[offset] & 0xff) << 24)
             | ((long) (bytes[offset + 1] & 0xff) << 16)
             | ((long) (bytes[offset + 2] & 0xff) << 8)
             | (long) (bytes[offset + 3] & 0xff);
    }

    private byte[] read(long start, int length) throws Exception {
        byte[] bytes = new byte[length];
        currentProgram.getMemory().getBytes(toAddr(start), bytes);
        return bytes;
    }

    private static String hex(byte[] bytes) {
        StringBuilder result = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) {
            result.append(String.format("%02x", value & 0xff));
        }
        return result.toString();
    }

    private void qualifyIdentity() {
        if (!PROJECT.equals(state.getProject().getName())) {
            throw new AssertionError("wrong project: " + state.getProject().getName());
        }
        if (!PROGRAM.equals(currentProgram.getName())) {
            throw new AssertionError("wrong program: " + currentProgram.getName());
        }
        if (!XEX_SHA256.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
            throw new AssertionError("wrong XEX: " + currentProgram.getExecutableSHA256());
        }
        String language = currentProgram.getLanguageID().toString();
        if (!LANGUAGE.equals(language) && !(LANGUAGE + ":default").equals(language)) {
            throw new AssertionError("wrong language: " + language);
        }
    }

    private long extent(byte[] pdata, long entry) {
        for (int offset = 0; offset + 8 <= pdata.length; offset += 8) {
            if (beU32(pdata, offset) == entry) {
                return ((beU32(pdata, offset + 4) >>> 8) & 0x3fffffL) * 4;
            }
        }
        return 0;
    }

    private void qualifyCandidate(byte[] pdata, long entry) throws Exception {
        long length = extent(pdata, entry);
        if (length <= 0 || length > 0x10000L) {
            throw new AssertionError(String.format(
                "missing/invalid .pdata extent for 0x%08X", entry));
        }
        Function function = getFunctionAt(toAddr(entry));
        if (function == null) {
            throw new AssertionError(String.format("no function at 0x%08X", entry));
        }

        Address start = toAddr(entry);
        Address end = toAddr(entry + length - 1);
        AddressSet extentSet = new AddressSet(start, end);
        InstructionIterator instructions =
            currentProgram.getListing().getInstructions(extentSet, true);
        int instructionCount = 0;
        int indirectCalls = 0;
        Map<Long, Integer> directCalls = new TreeMap<>();
        while (instructions.hasNext()) {
            Instruction instruction = instructions.next();
            ++instructionCount;
            if (!instruction.getFlowType().isCall()) {
                continue;
            }
            Address[] flows = instruction.getFlows();
            if (flows.length == 0) {
                ++indirectCalls;
            } else {
                for (Address flow : flows) {
                    directCalls.merge(flow.getOffset(), 1, Integer::sum);
                }
            }
        }

        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(start);
        int referenceCount = 0;
        StringBuilder referenceSites = new StringBuilder();
        while (references.hasNext()) {
            Reference reference = references.next();
            ++referenceCount;
            if (referenceCount <= 24) {
                if (referenceSites.length() != 0) {
                    referenceSites.append(',');
                }
                referenceSites.append(reference.getFromAddress());
            }
        }

        println(String.format(
            "AC6_US_WORLD_OWNER address=0x%08X end_exclusive=0x%08X length=%d "
            + "entry_bytes=%s ghidra_name=%s ghidra_body=%s instructions=%d "
            + "indirect_calls=%d incoming_refs=%d incoming_sites=%s",
            entry, entry + length, length, hex(read(entry, 32)),
            function.getName(), function.getBody(), instructionCount,
            indirectCalls, referenceCount, referenceSites));
        for (Map.Entry<Long, Integer> call : directCalls.entrySet()) {
            println(String.format(
                "AC6_US_WORLD_OWNER_CALL owner=0x%08X target=0x%08X count=%d",
                entry, call.getKey(), call.getValue()));
        }
    }

    @Override
    protected void run() throws Exception {
        qualifyIdentity();
        byte[] pdata = read(PDATA_START, PDATA_BYTES);
        println("AC6_US_WORLD_OWNER_IDENTITY project=" + PROJECT
            + " program=" + PROGRAM + " language=" + LANGUAGE
            + " xex_sha256=" + XEX_SHA256);
        for (long candidate : CANDIDATES) {
            qualifyCandidate(pdata, candidate);
        }
        println("AC6_US_WORLD_OWNER_PASS candidates=5");
    }
}
