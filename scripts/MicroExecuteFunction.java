// Micro-execute one retail function on a described state and emit a snapshot.
//
// MicroExecuteScenarioParser.java does this for one shape only: a parser, with
// a payload in, and a record and a buffer out, at three hard-coded bases and
// three hard-coded integer arguments. That shape is the *Bin readers' shape and
// nothing else's. A gameplay function has another one - a `this` pointer, a
// float delta, an input state, and writes scattered through the object it is
// handed - so the harness is parameterised here instead of copied.
//
// What is deliberately unchanged, because the committed evidence depends on it:
//
//   * the synthetic address space sits outside the program image, so a stray
//     write is obvious rather than silently landing in real data;
//   * written bytes come from the union of two poison passes, 0xCD and 0x00,
//     because a function can legitimately write a byte equal to any single
//     poison (cycle 1090);
//   * the emitted document is `ac6.function-snapshot.v1`, so
//     tools/compare_ac6_function_snapshots.py and
//     tools/emit_ac6_reader_digests.py consume it with no change.
//
// The exported high p-code in exports/*.json cannot be used for this. It is SSA
// form and carries MULTIEQUAL phi nodes, so it is not linearly executable.
// EmulatorHelper runs the raw p-code of each instruction instead, which is both
// simpler and more faithful.
//
// Usage:
//   -postScript MicroExecuteFunction.java SPEC OUT_JSON
//   -postScript MicroExecuteFunction.java --batch MANIFEST
// where MANIFEST holds one `SPEC OUT_JSON` pair per line. Batch mode exists
// because a matrix of gameplay cases is the normal shape of a question here,
// and paying Ghidra's startup once per case instead of once per matrix is the
// difference between a usable instrument and an unusable one.
// Read-only with respect to the project. Run with -readOnly -noanalysis.
//
// The spec is line-oriented, `#` starts a comment, and the directives are:
//
//   function ADDR              entry point, hex
//   case TEXT                  the case label; both sides must spell it alike
//   steps N                    step ceiling (default 400000)
//   region NAME BASE KIND      KIND is file:PATH | poison:SIZE | zero:SIZE
//   gpr rN VALUE               integer argument or seed register
//   fpr fN VALUE               float argument; f:<double> or raw 0x... bits
//   sp VALUE                   stack pointer; defaults to the top zero region
//   stub ADDR NOTE             intercept the call, record it, return via LR
//   capture gpr:rN | fpr:fN    register recorded in `registers` and compared
//
// VALUE accepts `0x...`, a decimal integer, or `REGION+0x...` / `REGION` to
// name a region base. Only `poison:` regions are write-detected: a region the
// function is merely reading has nothing to report and would drown the diff.
// @category AC6

import ghidra.app.emulator.EmulatorHelper;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import java.io.PrintWriter;
import java.math.BigInteger;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

public class MicroExecuteFunction extends GhidraScript {

    private static final String QUALIFIED_XEX_SHA256 =
        "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";

    private static final long RETURN_SENTINEL = 0x00DEAD00L;
    private static final byte POISON_A = (byte) 0xCD;
    private static final byte POISON_B = (byte) 0x00;
    private static final int DEFAULT_MAX_STEPS = 400000;

    /** One named span of the synthetic address space. */
    private static final class Region {
        String name;
        long base;
        String kind;       // "file", "poison", "zero"
        int size;
        byte[] contents;   // file regions only
        String sha256;     // file regions only
    }

    private final Map<String, Region> regions = new LinkedHashMap<>();
    private final Map<String, String> gprSeeds = new LinkedHashMap<>();
    private final Map<String, String> fprSeeds = new LinkedHashMap<>();
    private final Map<Long, String> stubs = new LinkedHashMap<>();
    private final List<String> captureGpr = new ArrayList<>();
    private final List<String> captureFpr = new ArrayList<>();
    private final List<String> calls = new ArrayList<>();
    private final Set<Long> stubbed = new LinkedHashSet<>();

    private long functionAddress;
    private String caseLabel;
    private String stackSpec;
    private int maxSteps = DEFAULT_MAX_STEPS;

    private int calleeEntries;
    private int lastSteps;
    private String lastExitKind = "return";
    private String lastExitDetail = "";
    private final Map<String, String> capturedValues = new LinkedHashMap<>();

    /**
     * Every field a spec sets, cleared. Batch mode runs specs in one process,
     * so a case inheriting the previous case's region or stub would be a silent
     * wrong answer rather than a failure.
     */
    private void resetCase() {
        regions.clear();
        gprSeeds.clear();
        fprSeeds.clear();
        stubs.clear();
        captureGpr.clear();
        captureFpr.clear();
        calls.clear();
        stubbed.clear();
        capturedValues.clear();
        functionAddress = 0;
        caseLabel = null;
        stackSpec = null;
        maxSteps = DEFAULT_MAX_STEPS;
        calleeEntries = 0;
        lastSteps = 0;
        lastExitKind = "return";
        lastExitDetail = "";
    }

    // ---------------------------------------------------------------- helpers

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

    /**
     * A spec value: a literal, or a region base with an optional displacement.
     * Region-relative form exists so a spec never repeats a base, which is the
     * way a spec and the harness silently disagree.
     */
    private long resolveValue(String token) {
        String text = token.trim();
        int plus = text.indexOf('+');
        String head = plus < 0 ? text : text.substring(0, plus);
        long displacement = 0;
        if (plus >= 0) {
            displacement = Long.decode(text.substring(plus + 1).trim()) & 0xffffffffL;
        }
        Region region = regions.get(head);
        if (region != null) {
            return (region.base + displacement) & 0xffffffffL;
        }
        if (plus >= 0) {
            throw new IllegalArgumentException("unknown region in value: " + token);
        }
        return Long.decode(text) & 0xffffffffL;
    }

    /** A float seed: `f:<double>` for a value, `0x...` for raw 64-bit bits. */
    private static BigInteger resolveFloat(String token) {
        String text = token.trim();
        if (text.startsWith("f:") || text.startsWith("F:")) {
            long bits = Double.doubleToRawLongBits(Double.parseDouble(text.substring(2)));
            return new BigInteger(Long.toUnsignedString(bits));
        }
        return new BigInteger(text.startsWith("0x") || text.startsWith("0X")
            ? text.substring(2) : text, text.startsWith("0x") || text.startsWith("0X") ? 16 : 10);
    }

    private void readSpec(String path) throws Exception {
        for (String rawLine : Files.readAllLines(Paths.get(path))) {
            String line = rawLine;
            int comment = line.indexOf('#');
            if (comment >= 0) {
                line = line.substring(0, comment);
            }
            line = line.trim();
            if (line.isEmpty()) {
                continue;
            }
            String[] parts = line.split("\\s+");
            switch (parts[0]) {
                case "function":
                    functionAddress = Long.decode(parts[1]) & 0xffffffffL;
                    break;
                case "case":
                    caseLabel = line.substring(line.indexOf(parts[1]));
                    break;
                case "steps":
                    maxSteps = Integer.decode(parts[1]);
                    break;
                case "region": {
                    Region region = new Region();
                    region.name = parts[1];
                    region.base = Long.decode(parts[2]) & 0xffffffffL;
                    String kind = parts[3];
                    int colon = kind.indexOf(':');
                    region.kind = kind.substring(0, colon);
                    String argument = kind.substring(colon + 1);
                    if ("file".equals(region.kind)) {
                        region.contents = Files.readAllBytes(Paths.get(argument));
                        region.size = region.contents.length;
                        region.sha256 = sha256(region.contents);
                    }
                    else if ("poison".equals(region.kind) || "zero".equals(region.kind)) {
                        region.size = Integer.decode(argument);
                    }
                    else {
                        throw new IllegalArgumentException("unknown region kind: " + region.kind);
                    }
                    regions.put(region.name, region);
                    break;
                }
                case "gpr":
                    gprSeeds.put(parts[1], parts[2]);
                    break;
                case "fpr":
                    fprSeeds.put(parts[1], parts[2]);
                    break;
                case "sp":
                    stackSpec = parts[1];
                    break;
                case "stub":
                    stubs.put(Long.decode(parts[1]) & 0xffffffffL,
                        parts.length > 2 ? line.substring(line.indexOf(parts[2])) : "stubbed call");
                    break;
                case "capture":
                    for (int index = 1; index < parts.length; ++index) {
                        String what = parts[index];
                        if (what.startsWith("gpr:")) {
                            captureGpr.add(what.substring(4));
                        }
                        else if (what.startsWith("fpr:")) {
                            captureFpr.add(what.substring(4));
                        }
                        else {
                            throw new IllegalArgumentException("unknown capture: " + what);
                        }
                    }
                    break;
                default:
                    throw new IllegalArgumentException("unknown directive: " + parts[0]);
            }
        }
        if (functionAddress == 0 || caseLabel == null) {
            throw new IllegalArgumentException("spec needs both `function` and `case`");
        }
    }

    /** Contiguous spans a poison region reports as written, in address order. */
    private int writtenRanges(Region region, byte[] valuesA, byte[] valuesB,
            List<String> writes) {
        int length = region.size;
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
            writes.add(String.format(
                "{\"address\": \"0x%08x\", \"size\": %d, \"after_hex\": \"%s\"}",
                region.base + start, slice.length, hex(slice)));
        }
        return total;
    }

    // ------------------------------------------------------------- the engine

    /** One complete emulation with a given poison fill; returns each poison region's bytes. */
    private Map<String, byte[]> emulationPass(byte poison, boolean record) throws Exception {
        EmulatorHelper emulator = new EmulatorHelper(currentProgram);
        try {
            String pc = emulator.getPCRegister().getName();
            String lr = registerName("LR", "lr");
            String sp = registerName("r1");

            for (Region region : regions.values()) {
                if ("file".equals(region.kind)) {
                    emulator.writeMemory(toAddr(region.base), region.contents);
                }
                else {
                    byte[] fill = new byte[region.size];
                    java.util.Arrays.fill(fill, "poison".equals(region.kind) ? poison : (byte) 0);
                    emulator.writeMemory(toAddr(region.base), fill);
                }
            }

            if (stackSpec != null) {
                emulator.writeRegister(sp, resolveValue(stackSpec));
            }
            for (Map.Entry<String, String> seed : gprSeeds.entrySet()) {
                emulator.writeRegister(seed.getKey(), resolveValue(seed.getValue()));
            }
            for (Map.Entry<String, String> seed : fprSeeds.entrySet()) {
                emulator.writeRegister(registerName(seed.getKey(),
                    seed.getKey().toUpperCase()), resolveFloat(seed.getValue()));
            }
            emulator.writeRegister(lr, RETURN_SENTINEL);
            emulator.writeRegister(pc, functionAddress);

            int steps = 0;
            String exitKind = "return";
            String exitDetail = "";
            while (steps < maxSteps) {
                long here = emulator.readRegister(pc).longValue() & 0xffffffffL;
                if (here == RETURN_SENTINEL) {
                    break;
                }
                String note = stubs.get(here);
                if (note != null) {
                    // Which stubbed path fired is an observable; executing the
                    // callee would wander into varargs and platform state.
                    if (record) {
                        long argument = emulator.readRegister("r3").longValue() & 0xffffffffL;
                        calls.add(String.format(
                            "{\"target\": \"0x%08x\", \"ordinal\": %d, \"note\": \"%s, arg 0x%08x\"}",
                            here, calls.size(), note, argument));
                    }
                    stubbed.add(here);
                    emulator.writeRegister(pc,
                        emulator.readRegister(lr).longValue() & 0xffffffffL);
                    steps++;
                    continue;
                }
                if (record && here != functionAddress && getFunctionAt(toAddr(here)) != null) {
                    // Entering a callee is an implementation detail of the
                    // machine code, not an observable. Counted for provenance,
                    // never compared.
                    calleeEntries++;
                }
                if (!emulator.step(monitor)) {
                    exitKind = "fault";
                    exitDetail = String.valueOf(emulator.getLastError());
                    break;
                }
                steps++;
            }
            if (steps >= maxSteps) {
                exitKind = "step_limit";
            }

            Map<String, byte[]> result = new LinkedHashMap<>();
            for (Region region : regions.values()) {
                if ("poison".equals(region.kind)) {
                    result.put(region.name, emulator.readMemory(toAddr(region.base), region.size));
                }
            }
            if (record) {
                lastSteps = steps;
                lastExitKind = exitKind;
                lastExitDetail = exitDetail;
                capturedValues.clear();
                for (String name : captureGpr) {
                    capturedValues.put(name, String.format("0x%08x",
                        emulator.readRegister(name).longValue() & 0xffffffffL));
                }
                for (String name : captureFpr) {
                    // Raw bits, not a decoded double: a formatted double is a
                    // second place for the two sides to disagree about nothing.
                    capturedValues.put(name, String.format("0x%016x",
                        emulator.readRegister(registerName(name, name.toUpperCase()))
                            .longValue()));
                }
            }
            return result;
        }
        finally {
            emulator.dispose();
        }
    }

    @Override
    protected void run() throws Exception {
        String sha = currentProgram.getExecutableSHA256();
        if (!QUALIFIED_XEX_SHA256.equalsIgnoreCase(sha)) {
            throw new IllegalStateException("unexpected XEX SHA-256: " + sha);
        }
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException(
                "usage: MicroExecuteFunction SPEC OUT_JSON | --batch MANIFEST");
        }
        if ("--batch".equals(args[0])) {
            int done = 0;
            for (String rawLine : Files.readAllLines(Paths.get(args[1]))) {
                String line = rawLine;
                int comment = line.indexOf('#');
                if (comment >= 0) {
                    line = line.substring(0, comment);
                }
                line = line.trim();
                if (line.isEmpty()) {
                    continue;
                }
                String[] pair = line.split("\\s+");
                if (pair.length != 2) {
                    throw new IllegalArgumentException("manifest line is not `SPEC OUT`: " + line);
                }
                executeOne(pair[0], pair[1], sha);
                done++;
            }
            println("AC6_MICROEXEC_BATCH cases=" + done + " manifest=" + args[1]);
            return;
        }
        executeOne(args[0], args[1], sha);
    }

    private void executeOne(String specPath, String outPath, String sha) throws Exception {
        resetCase();
        readSpec(specPath);

        Map<String, byte[]> passA = emulationPass(POISON_A, true);
        Map<String, byte[]> passB = emulationPass(POISON_B, false);

        // Address order across every poison region, which is what the digest in
        // tools/emit_ac6_reader_digests.py is defined on.
        List<Region> poisonRegions = new ArrayList<>();
        for (Region region : regions.values()) {
            if ("poison".equals(region.kind)) {
                poisonRegions.add(region);
            }
        }
        poisonRegions.sort((left, right) -> Long.compareUnsigned(left.base, right.base));

        List<String> writes = new ArrayList<>();
        StringBuilder writtenSummary = new StringBuilder();
        for (Region region : poisonRegions) {
            int bytes = writtenRanges(region, passA.get(region.name), passB.get(region.name),
                writes);
            if (writtenSummary.length() > 0) {
                writtenSummary.append(", ");
            }
            writtenSummary.append(region.name).append(' ').append(bytes).append(" bytes");
        }

        Function function = getFunctionAt(toAddr(functionAddress));
        StringBuilder json = new StringBuilder();
        json.append("{\n");
        json.append("  \"schema\": \"ac6.function-snapshot.v1\",\n");
        json.append("  \"identity\": {\n");
        json.append("    \"implementation\": \"ppc-pcode\",\n");
        json.append(String.format("    \"function\": \"0x%08X\",%n", functionAddress));
        json.append(String.format("    \"case\": \"%s\"%n", caseLabel));
        json.append("  },\n");
        json.append("  \"provenance\": {\n");
        json.append(String.format("    \"xex_sha256\": \"%s\",%n", sha));
        json.append(String.format("    \"function_name\": \"%s\",%n",
            function == null ? "<no function>" : function.getName()));
        json.append("    \"regions\": [");
        boolean first = true;
        for (Region region : regions.values()) {
            json.append(first ? "\n      " : ",\n      ");
            first = false;
            json.append(String.format(
                "{\"name\": \"%s\", \"base\": \"0x%08x\", \"kind\": \"%s\", \"size\": %d%s}",
                region.name, region.base, region.kind, region.size,
                region.sha256 == null ? "" : ", \"sha256\": \"" + region.sha256 + "\""));
        }
        json.append(regions.isEmpty() ? "]," : "\n    ],").append("\n");
        json.append(String.format("    \"steps\": %d,%n", lastSteps));
        json.append(String.format("    \"callee_entries\": %d,%n", calleeEntries));
        json.append("    \"write_detection\": \"union of two poison passes, 0xCD and 0x00\",\n");
        json.append(String.format("    \"written\": \"%s\"%n", writtenSummary));
        json.append("  },\n");
        json.append(String.format("  \"exit\": {\"kind\": \"%s\"%s},%n", lastExitKind,
            lastExitDetail.isEmpty() ? "" : ", \"detail\": \"" + lastExitDetail + "\""));
        json.append("  \"registers\": {");
        first = true;
        for (Map.Entry<String, String> entry : capturedValues.entrySet()) {
            json.append(first ? "\n    " : ",\n    ");
            first = false;
            json.append(String.format("\"%s\": \"%s\"", entry.getKey(), entry.getValue()));
        }
        json.append(capturedValues.isEmpty() ? "}," : "\n  },").append("\n");
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
            + " case=" + caseLabel
            + " steps=" + lastSteps + " exit=" + lastExitKind
            + " callee_entries=" + calleeEntries + " stubbed_calls=" + calls.size()
            + " written=" + writtenSummary
            + " out=" + outPath);
    }
}
