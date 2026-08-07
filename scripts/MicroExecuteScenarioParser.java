// Micro-execute one scenario parser on real retail bytes and emit a snapshot.
//
// This is the `microexec` evidence the Mission 01 v2 gate accepts. It executes
// the actual PPC instruction semantics through Ghidra's p-code emulator, on the
// actual Mission 01 payload, with no emulator, bridge or native run involved:
// one function, a synthetic heap, a few thousand steps.
//
// The exported high p-code in exports/*.json cannot be used for this. It is SSA
// form and carries MULTIEQUAL phi nodes, so it is not linearly executable.
// EmulatorHelper runs the raw p-code of each instruction instead, which is both
// simpler and more faithful.
//
// Memory layout, chosen to sit outside the program image so a stray write is
// obvious rather than silently landing in real data:
//   payload  0xB0000000  the decoded scenario node graph, loaded verbatim
//   record   0xB4000000  the destination record, poison-filled
//   buffer   0xB5000000  the sub-record buffer, poison-filled
//   stack    0xC0001000  grows down
//
// Calls into the error printer are stubbed and recorded rather than executed:
// which fail-closed path a parser takes is exactly what the snapshot should
// capture. Calls into other parsers run for real.
//
// Usage:
//   -postScript MicroExecuteScenarioParser.java FUNC NODE_OFFSET PAYLOAD OUT_JSON
// Read-only with respect to the project. Run with -readOnly -noanalysis.
// @category AC6

import ghidra.app.emulator.EmulatorHelper;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import java.io.PrintWriter;
import java.math.BigInteger;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

public class MicroExecuteScenarioParser extends GhidraScript {

    private static final String QUALIFIED_XEX_SHA256 =
        "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";

    private static final long PAYLOAD_BASE = 0xB0000000L;
    private static final long RECORD_BASE  = 0xB4000000L;
    private static final long BUFFER_BASE  = 0xB5000000L;
    private static final long STACK_TOP    = 0xC0001000L;
    private static final long RETURN_SENTINEL = 0x00DEAD00L;

    private static final int RECORD_BYTES = 0x100;
    private static final int BUFFER_BYTES = 0x8000;
    // Two poison values, not one. A byte the parser legitimately writes can
    // equal any single poison, which would make it look unwritten and split the
    // reported run. Running twice and taking the union of the bytes that differ
    // from their own poison removes that blind spot entirely.
    private static final byte POISON_A = (byte) 0xCD;
    private static final byte POISON_B = (byte) 0x00;

    private static final int MAX_STEPS = 400000;

    // The error printer never returns anything the parsers use; executing it
    // would wander into varargs and platform state.
    private static final long ERROR_PRINTER = 0x823828B8L;

    private EmulatorHelper emulator;
    private final List<String> calls = new ArrayList<>();
    private int subCalls;
    private int lastSteps;
    private String lastExitKind = "return";
    private String lastExitDetail = "";
    private long lastR3;
    private final Set<Long> stubbed = new LinkedHashSet<>();

    private String registerName(String... candidates) {
        for (String candidate : candidates) {
            Register register = currentProgram.getLanguage().getRegister(candidate);
            if (register != null) {
                return register.getName();
            }
        }
        throw new IllegalStateException("no register found among the candidates");
    }

    private static String hex(byte[] bytes) {
        StringBuilder text = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) {
            text.append(String.format("%02x", value & 0xff));
        }
        return text.toString();
    }

    private static String sha256(byte[] bytes) throws Exception {
        return hex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }

    /** Contiguous spans the run wrote, from the union of both poison passes. */
    private String writtenRanges(long base, int length, byte[] valuesA, byte[] valuesB,
            List<String> writes) {
        boolean[] written = new boolean[length];
        for (int index = 0; index < length; ++index) {
            written[index] = valuesA[index] != POISON_A || valuesB[index] != POISON_B;
        }
        int index = 0;
        int total = 0;
        while (index < length) {
            if (!written[index]) {
                index++;
                continue;
            }
            int start = index;
            while (index < length && written[index]) {
                index++;
            }
            byte[] slice = new byte[index - start];
            System.arraycopy(valuesA, start, slice, 0, slice.length);
            total += slice.length;
            writes.add(String.format("{\"address\": \"0x%08x\", \"size\": %d, \"after_hex\": \"%s\"}",
                base + start, slice.length, hex(slice)));
        }
        return total + " bytes";
    }

    @Override
    protected void run() throws Exception {
        String sha = currentProgram.getExecutableSHA256();
        if (!QUALIFIED_XEX_SHA256.equalsIgnoreCase(sha)) {
            throw new IllegalStateException("unexpected XEX SHA-256: " + sha);
        }
        String[] args = getScriptArgs();
        if (args.length != 4 && args.length != 5) {
            throw new IllegalArgumentException(
                "usage: MicroExecuteScenarioParser FUNC NODE_OFFSET PAYLOAD OUT_JSON [CLASS]");
        }
        long functionAddress = Long.decode(args[0]) & 0xffffffffL;
        String[] nodeList = args[1].split(",");
        byte[] payload = Files.readAllBytes(Paths.get(args[2]));

        Function function = getFunctionAt(toAddr(functionAddress));
        // The case label must match the native snapshot's, and Ghidra's own
        // FUN_xxxxxxxx name would not. Take the class name from the caller.
        String functionName = args.length == 5 ? args[4]
            : (function == null ? "<none>" : function.getName());

        // One emulator per node: a fresh memory state per case keeps each
        // snapshot independent of the one before it.
        for (String nodeText : nodeList) {
            long nodeOffset = Long.decode(nodeText.trim()) & 0xffffffffL;
            String outPath = nodeList.length == 1 ? args[3]
                : args[3].replace(".ppc.json", "-" + Long.toHexString(nodeOffset) + ".ppc.json");
            calls.clear();
            subCalls = 0;
            executeOne(functionAddress, nodeOffset, payload, outPath, functionName, sha);
        }
    }

    /** One complete emulation with a given poison fill; returns record+buffer. */
    private byte[][] emulationPass(long functionAddress, long nodeOffset, byte[] payload,
            byte poison, boolean recordCalls) throws Exception {
        emulator = new EmulatorHelper(currentProgram);
        try {
            String pc = emulator.getPCRegister().getName();
            String lr = registerName("LR", "lr");
            String sp = registerName("r1");

            emulator.writeMemory(toAddr(PAYLOAD_BASE), payload);

            byte[] poisonRecord = new byte[RECORD_BYTES];
            byte[] poisonBuffer = new byte[BUFFER_BYTES];
            java.util.Arrays.fill(poisonRecord, poison);
            java.util.Arrays.fill(poisonBuffer, poison);
            emulator.writeMemory(toAddr(RECORD_BASE), poisonRecord);
            emulator.writeMemory(toAddr(BUFFER_BASE), poisonBuffer);
            emulator.writeMemory(toAddr(STACK_TOP - 0x1000), new byte[0x1000]);

            emulator.writeRegister(sp, STACK_TOP - 0x200);
            emulator.writeRegister("r3", RECORD_BASE);
            emulator.writeRegister("r4", PAYLOAD_BASE + nodeOffset);
            emulator.writeRegister("r5", BUFFER_BASE);
            emulator.writeRegister(lr, RETURN_SENTINEL);
            emulator.writeRegister(pc, functionAddress);

            int steps = 0;
            String exitKind = "return";
            String exitDetail = "";
            while (steps < MAX_STEPS) {
                BigInteger current = emulator.readRegister(pc);
                long here = current.longValue() & 0xffffffffL;
                if (here == RETURN_SENTINEL) {
                    break;
                }
                if (here == ERROR_PRINTER) {
                    // Record which fail-closed path fired, then return.
                    long argument = emulator.readRegister("r3").longValue() & 0xffffffffL;
                    if (recordCalls) {
                        calls.add(String.format(
                            "{\"target\": \"0x%08x\", \"ordinal\": %d, \"note\": \"error printer, arg 0x%08x\"}",
                            ERROR_PRINTER, calls.size(), argument));
                    }
                    stubbed.add(ERROR_PRINTER);
                    long back = emulator.readRegister(lr).longValue() & 0xffffffffL;
                    emulator.writeRegister(pc, back);
                    steps++;
                    continue;
                }
                Function entered = getFunctionAt(toAddr(here));
                if (entered != null && here != functionAddress && recordCalls) {
                    // Entering a sub-parser is an implementation detail of the
                    // machine code, not an observable of the parsed result. It
                    // is counted for provenance, never compared.
                    subCalls++;
                }
                if (!emulator.step(monitor)) {
                    exitKind = "fault";
                    exitDetail = String.valueOf(emulator.getLastError());
                    break;
                }
                steps++;
            }
            if (steps >= MAX_STEPS) {
                exitKind = "step_limit";
            }
            if (recordCalls) {
                lastSteps = steps;
                lastExitKind = exitKind;
                lastExitDetail = exitDetail;
                lastR3 = emulator.readRegister("r3").longValue() & 0xffffffffL;
            }
            return new byte[][] {
                emulator.readMemory(toAddr(RECORD_BASE), RECORD_BYTES),
                emulator.readMemory(toAddr(BUFFER_BASE), BUFFER_BYTES),
            };
        } finally {
            emulator.dispose();
        }
    }

    private void executeOne(long functionAddress, long nodeOffset, byte[] payload,
            String outPath, String functionName, String sha) throws Exception {
        byte[][] passA = emulationPass(functionAddress, nodeOffset, payload, POISON_A, true);
        byte[][] passB = emulationPass(functionAddress, nodeOffset, payload, POISON_B, false);

        {
            int steps = lastSteps;
            String exitKind = lastExitKind;
            String exitDetail = lastExitDetail;

            List<String> writes = new ArrayList<>();
            String recordWritten =
                writtenRanges(RECORD_BASE, RECORD_BYTES, passA[0], passB[0], writes);
            String bufferWritten =
                writtenRanges(BUFFER_BASE, BUFFER_BYTES, passA[1], passB[1], writes);

            StringBuilder json = new StringBuilder();
            json.append("{\n");
            json.append("  \"schema\": \"ac6.function-snapshot.v1\",\n");
            json.append("  \"identity\": {\n");
            json.append("    \"implementation\": \"ppc-pcode\",\n");
            json.append(String.format("    \"function\": \"0x%08X\",%n", functionAddress));
            json.append(String.format("    \"case\": \"%s@node+0x%x\"%n", functionName, nodeOffset));
            json.append("  },\n");
            json.append("  \"provenance\": {\n");
            json.append(String.format("    \"xex_sha256\": \"%s\",%n", sha));
            json.append(String.format("    \"payload_sha256\": \"%s\",%n", sha256(payload)));
            json.append(String.format("    \"payload_base\": \"0x%08x\",%n", PAYLOAD_BASE));
            json.append(String.format("    \"record_base\": \"0x%08x\",%n", RECORD_BASE));
            json.append(String.format("    \"buffer_base\": \"0x%08x\",%n", BUFFER_BASE));
            json.append(String.format("    \"steps\": %d,%n", steps));
            json.append(String.format("    \"sub_parser_entries\": %d,%n", subCalls));
            json.append(String.format("    \"r3_on_return\": \"0x%08x\",%n", lastR3));
            json.append("    \"write_detection\": \"union of two poison passes, 0xCD and 0x00\",\n");
            json.append("    \"r3_note\": \"scratch after a void reader; not compared\",\n");
            json.append(String.format("    \"record_written\": \"%s\",%n", recordWritten));
            json.append(String.format("    \"buffer_written\": \"%s\"%n", bufferWritten));
            json.append("  },\n");
            json.append(String.format("  \"exit\": {\"kind\": \"%s\"%s},%n", exitKind,
                exitDetail.isEmpty() ? "" : ", \"detail\": \"" + exitDetail + "\""));
            json.append("  \"registers\": {},\n");
            json.append("  \"calls\": [");
            for (int index = 0; index < calls.size(); ++index) {
                json.append(index == 0 ? "\n    " : ",\n    ").append(calls.get(index));
            }
            json.append(calls.isEmpty() ? "]," : "\n  ],").append("\n");
            json.append("  \"memory_writes\": [");
            for (int index = 0; index < writes.size(); ++index) {
                json.append(index == 0 ? "\n    " : ",\n    ").append(writes.get(index));
            }
            json.append(writes.isEmpty() ? "]" : "\n  ]").append("\n");
            json.append("}\n");

            try (PrintWriter out = new PrintWriter(outPath)) {
                out.print(json);
            }
            println("AC6_MICROEXEC function=0x" + Long.toHexString(functionAddress)
                + " name=" + functionName
                + " node=+0x" + Long.toHexString(nodeOffset)
                + " steps=" + steps + " exit=" + exitKind
                + " sub_calls=" + subCalls + " error_calls=" + calls.size()
                + " record=" + recordWritten + " buffer=" + bufferWritten
                + " out=" + outPath);
        }
    }
}
