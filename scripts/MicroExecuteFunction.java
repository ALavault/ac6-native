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
//                              | bytes:HEX, an inline literal for small fixtures
//   gpr rN VALUE               integer argument or seed register
//   fpr fN VALUE               float argument; f:<double> or raw 0x... bits
//   vec NAME HEX               seed a vector register, 32 hex digits
//   sp VALUE                   stack pointer; defaults to the top zero region
//   stub ADDR NOTE             intercept the call, record it, return via LR
//   hint NAME                  a p-code op with no architectural effect (dcbt and
//                              friends) becomes a no-op; counted apart from
//                              asserted semantics, because a supplied nothing is
//                              not a supplied model
//   alias on                   bridge the two vector register files (cycle 1301)
//   dump NAME                  emit a region's FINAL BYTES, both poison passes,
//                              under `region_dumps`. For a region seeded with
//                              `bytes:` this is the only way to read it back --
//                              write detection needs a poison fill, and a region
//                              that has to be pre-filled cannot have one. The
//                              two passes are emitted separately so a reader can
//                              see for itself that a seeded region's result does
//                              not depend on the poison, rather than trust it.
//   override ADDR NAME         replace an instruction the module implements
//                              wrongly; counted under asserted_semantics
//   capture gpr:rN | fpr:fN | vec:NAME
//                              register recorded in `registers` and compared
//
// VALUE accepts `0x...`, a decimal integer, or `REGION+0x...` / `REGION` to
// name a region base. Only `poison:` regions are write-detected: a region the
// function is merely reading has nothing to report and would drown the diff.
// @category AC6

import ghidra.app.emulator.EmulatorHelper;
import ghidra.app.script.GhidraScript;
import ghidra.pcode.emulate.BreakCallBack;
import ghidra.pcode.memstate.MemoryState;
import ghidra.pcode.pcoderaw.PcodeOpRaw;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.pcode.Varnode;
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
    private final Map<String, String> vecSeeds = new LinkedHashMap<>();
    private final Map<Long, String> stubs = new LinkedHashMap<>();
    private final List<String> captureGpr = new ArrayList<>();
    private final List<String> captureFpr = new ArrayList<>();
    private final List<String> captureVec = new ArrayList<>();
    private final List<String> dumpRegions = new ArrayList<>();
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

    // ------------------------------------------------------- asserted semantics
    //
    // The Xenon SLEIGH module decodes VMX128 and gives part of it no executable
    // semantics, emitting a CALLOTHER the emulator refuses (cycle 1294: 70
    // distinct operations, 15,945 sites). These supply five of them.
    //
    // THIS IS A WEAKER KIND OF EVIDENCE AND THE SNAPSHOT SAYS SO. Everything
    // else the harness produces is retail instructions executing; this is my
    // model of an instruction, from the architecture's documentation, standing
    // in for one. A snapshot that used it records which operations fired and how
    // often, under `asserted_semantics`, so no reader can mistake the two.
    //
    // The operand order of each is not taken from documentation - it is what
    // this SLEIGH build passes, read out of the p-code with
    // scripts/Ac6PcodeDump.java, because the module is free to choose and only
    // the build in use can say what it chose:
    //
    //   vectorMergeHighWord(vA:16, vB:16) -> vD:16
    //   vectorMergeLowWord(vA:16, vB:16) -> vD:16
    //   loadVectorLeftIndexed128(rA:8, rB:8) -> vD:16
    //   vectorRotateLeftImmediateMaskInsert128(vD:16, vB:16, imm:1, z:1) -> vD:16
    //   loadVectorForShiftLeft(rA:8, rB:8) -> vD:16
    //
    // Cross-references for the semantics themselves: the Cell BE SIMD PEM
    // (v2.07c) for lvlx, which is where the lvlx/lvrx family is documented at
    // all, and Xenia's ppc_emit_altivec.cc for vrlimi128's mask and rotate
    // encoding. Reading either for what an instruction means is documentation,
    // not an oracle pass: no game code runs and no game behaviour is observed.

    private boolean vmxEnabled;
    private final Map<String, Integer> assertedFired = new LinkedHashMap<>();

    // ---------------------------------------------------------------- hints
    //
    // A cache hint is not asserted semantics. `dcbt` and its siblings are defined
    // by the architecture to have NO effect on program state -- they are
    // prefetches -- so a no-op reproduces them exactly rather than modelling
    // them. The emulator still refuses them, because the module emits a
    // CALLOTHER and gives it no behaviour.
    //
    // They are counted separately from `asserted_semantics` for that reason: a
    // reader must be able to tell a supplied instruction model from a supplied
    // nothing.
    private final Set<String> hintOps = new LinkedHashSet<>();
    private final Map<String, Integer> hintsFired = new LinkedHashMap<>();

    private BreakCallBack noOperation(String name) {
        return new BreakCallBack() {
            @Override
            public boolean pcodeCallback(PcodeOpRaw op) {
                hintsFired.merge(name, 1, Integer::sum);
                return true;
            }
        };
    }
    private final Map<Long, String> overrides = new LinkedHashMap<>();

    // ------------------------------------------------- instruction overrides
    //
    // A CALLOTHER can be replaced by registering a behaviour. An instruction the
    // module implements ITSELF cannot: there is no hook, and cycle 1297 measured
    // one such instruction returning the wrong answer -- `vpermwi128` permutes
    // with its lane selection reversed against the ISA.
    //
    // So the p-code is bypassed at the address instead, the same way a stubbed
    // call is: the step loop recognises the PC, runs this, and advances by four.
    //
    // WHAT IS REPLACED IS THE SEMANTICS, NOT THE DECODE. The module's decode is
    // corroborated for this instruction -- its own p-code materialises the
    // immediate as 0xac, and the register operands match the disassembly -- so
    // the operands are read back through the Instruction API rather than
    // re-derived from the encoding. Guessing the encoding by hand was tried and
    // the VMX128 reference's PERM field would not reassemble; the isolation test
    // in tools/audit_vmx128_behaviours.py is the control that covers the whole
    // chain, decode included, because a wrong register would read an unseeded
    // one and fail.

    // ------------------------------------------------ the register-file bridge
    //
    // Cycle 1301: on Xenon there are 128 vector registers and both instruction
    // families address the same ones -- the AltiVec forms as v0..v31, which this
    // module calls `vsNN`, and the VMX128 forms as vr0..vr127. `vs32+n` and
    // `vrn` are one storage on hardware and DISJOINT STORAGE in this module,
    // measured: `vspltw v5,v13,0x2` puts its splat in vs37 and leaves vr5 zero.
    //
    // So every value an AltiVec-form instruction produces is invisible to the
    // VMX128-form instruction that consumes it, and the dataflow of any real
    // routine is cut at each crossing. This copies each write to its alias,
    // which is what the hardware would have made unnecessary.
    //
    // Only the registers the instruction actually wrote are copied, taken from
    // its own result objects rather than by diffing the file: a blind mirror
    // would have to choose a direction and would get it wrong half the time.

    private boolean aliasEnabled;
    private int aliasCopies;

    /** `vs32+n` <-> `vrn`, or null when the register is not one of the pair. */
    private String aliasOf(String name) {
        try {
            if (name.startsWith("vs")) {
                int index = Integer.parseInt(name.substring(2));
                return (index >= 32 && index < 64) ? "vr" + (index - 32) : null;
            }
            if (name.startsWith("vr")) {
                int index = Integer.parseInt(name.substring(2));
                return (index >= 0 && index < 32) ? "vs" + (index + 32) : null;
            }
        }
        catch (NumberFormatException ignored) {
            return null;
        }
        return null;
    }

    private void mirrorWrites(ghidra.program.model.listing.Instruction instruction,
            EmulatorHelper emulator) {
        if (instruction == null) {
            return;
        }
        for (Object result : instruction.getResultObjects()) {
            if (!(result instanceof Register)) {
                continue;
            }
            String name = ((Register) result).getName();
            String alias = aliasOf(name);
            if (alias == null || currentProgram.getLanguage().getRegister(alias) == null) {
                continue;
            }
            emulator.writeRegister(alias, emulator.readRegister(name));
            aliasCopies++;
        }
    }

    private void applyOverride(String name, long address, EmulatorHelper emulator)
            throws Exception {
        ghidra.program.model.listing.Instruction instruction =
            getInstructionAt(toAddr(address));
        if (instruction == null) {
            throw new IllegalStateException("no instruction at " + Long.toHexString(address));
        }
        if (!"vpermwi128".equals(name) && !"vpermwi128-lowfirst".equals(name)) {
            throw new IllegalArgumentException("no override implemented for " + name);
        }
        // Two readings of one immediate, because the documentation carries both
        // (cycle 1305). `vpermwi128` follows Xenia's CODE -- the high bit-pair
        // selects element 0. `vpermwi128-lowfirst` follows Xenia's COMMENT read
        // in PowerPC bit numbering, and the SLEIGH module -- the low pair does.
        // Neither is asserted here; the spec chooses and the snapshot records
        // which fired.
        boolean lowFirst = name.endsWith("-lowfirst");
        // vpermwi128 vD, vB, uimm : VD.x = VB[uimm bits 6-7], VD.y = bits 4-5,
        // VD.z = bits 2-3, VD.w = bits 0-1. The HIGH pair selects the FIRST
        // word, which is the half the module has backwards.
        Register destination = (Register) instruction.getOpObjects(0)[0];
        Register source = (Register) instruction.getOpObjects(1)[0];
        int immediate = vpermwi128Immediate(instruction);

        // read/writeRegister rather than the memory state: it is the same API the
        // `vec` seeds and `capture vec:` use, so the byte order here is the one
        // the endianness anchor in audit_vmx128_behaviours.py already measured.
        String bits = emulator.readRegister(source.getName())
            .and(new BigInteger("ff".repeat(16), 16)).toString(16);
        String input = "0".repeat(32 - bits.length()) + bits;
        StringBuilder output = new StringBuilder(32);
        for (int lane = 0; lane < 4; ++lane) {
            int selector = lowFirst ? (immediate >> (2 * lane)) & 3
                                    : (immediate >> (2 * (3 - lane))) & 3;
            output.append(input, selector * 8, selector * 8 + 8);
        }
        emulator.writeRegister(destination.getName(), new BigInteger(output.toString(), 16));
        countFired("override:" + name);
    }

    /**
     * `vpermwi128`'s immediate, decoded from the instruction word.
     *
     * IT IS NOT TAKEN FROM THE MODULE, AND UNTIL CYCLE 1326 IT WAS. This
     * override existed to correct a lane order and read its immediate from
     * `Instruction.getOpObjects(2)` -- from the same module whose semantics it
     * was correcting. Cycle 1325 found the two decodes differing at three sites
     * and cycle 1326 graded them over the whole corpus: the module agrees with
     * XenonRecomp at **9 of 545 sites**.
     *
     * The layout is derived, not assumed. tools/audit_vpermwi128_immediate_decode.py
     * asks, for each of the eight immediate bits, which of the 32 instruction-word
     * bits agrees with it at EVERY site; each has exactly one answer, and the
     * result reproduces XenonRecomp 545/545 and the module 9/545:
     *
     *     imm[7..5] = word[23], word[24], word[25]      (PERMh)
     *     imm[4..0] = word[11..15]                      (PERMl)
     *
     * Word bits are numbered PowerPC style, 0 = most significant.
     */
    private static final int[] VPERMWI_IMMEDIATE_BITS = {15, 14, 13, 12, 11, 25, 24, 23};

    private int vpermwi128Immediate(ghidra.program.model.listing.Instruction instruction)
            throws Exception {
        byte[] bytes = instruction.getBytes();
        long word = 0;
        for (byte value : bytes) {
            word = (word << 8) | (value & 0xFFL);
        }
        int immediate = 0;
        for (int index = 0; index < VPERMWI_IMMEDIATE_BITS.length; ++index) {
            long bit = (word >> (31 - VPERMWI_IMMEDIATE_BITS[index])) & 1L;
            immediate |= (int) bit << index;
        }
        return immediate;
    }

    private byte[] readChunk(MemoryState memory, Varnode node) {
        byte[] bytes = new byte[node.getSize()];
        memory.getChunk(bytes, node.getAddress().getAddressSpace(), node.getOffset(),
            bytes.length, false);
        return bytes;
    }

    private void writeChunk(MemoryState memory, Varnode node, byte[] bytes) {
        memory.setChunk(bytes, node.getAddress().getAddressSpace(), node.getOffset(),
            bytes.length);
    }

    /**
     * The `(rA|0)` rule: in an indexed form, rA = r0 means the literal zero, not
     * the contents of r0. This module passes rA verbatim rather than resolving
     * it, so the rule is applied here, decided from the varnode's own identity.
     */
    private long baseRegisterValue(MemoryState memory, Varnode node) {
        if (node.isConstant()) {
            return node.getOffset();
        }
        Register register = currentProgram.getRegister(node.getAddress(), node.getSize());
        if (register != null && "r0".equals(register.getName())) {
            return 0;
        }
        return memory.getValue(node);
    }

    private void countFired(String name) {
        assertedFired.merge(name, 1, Integer::sum);
    }

    /** vD.word[2i] = vA.word[i + half], vD.word[2i+1] = vB.word[i + half]. */
    private BreakCallBack mergeWord(String name, int half) {
        return new BreakCallBack() {
            @Override
            public boolean pcodeCallback(PcodeOpRaw op) {
                MemoryState memory = emulate.getMemoryState();
                byte[] left = readChunk(memory, op.getInput(1));
                byte[] right = readChunk(memory, op.getInput(2));
                byte[] out = new byte[16];
                for (int word = 0; word < 2; ++word) {
                    System.arraycopy(left, (word + half) * 4, out, word * 8, 4);
                    System.arraycopy(right, (word + half) * 4, out, word * 8 + 4, 4);
                }
                writeChunk(memory, op.getOutput(), out);
                countFired(name);
                return true;
            }
        };
    }

    /**
     * Load Vector Left Indexed: the bytes from EA to the end of EA's 16-byte
     * block, left-justified, zero-filled. Reading `16 - eb` bytes at EA rather
     * than 16 at the aligned base is the same result and touches strictly less
     * memory, which matters when the state is a synthetic region.
     */
    private BreakCallBack loadVectorLeftIndexed() {
        return new BreakCallBack() {
            @Override
            public boolean pcodeCallback(PcodeOpRaw op) {
                MemoryState memory = emulate.getMemoryState();
                long address = (baseRegisterValue(memory, op.getInput(1))
                    + memory.getValue(op.getInput(2))) & 0xffffffffL;
                int eb = (int) (address & 0xF);
                byte[] out = new byte[16];
                byte[] loaded = new byte[16 - eb];
                memory.getChunk(loaded, defaultSpace(), address, loaded.length, false);
                System.arraycopy(loaded, 0, out, 0, loaded.length);
                writeChunk(memory, op.getOutput(), out);
                countFired("loadVectorLeftIndexed128");
                return true;
            }
        };
    }

    /**
     * Rotate vB left by `z` words, then take element i from that when bit `3-i`
     * of `imm` is set and from vD otherwise. The bit order is the one Xenia's
     * InstrEmit_vrlimi128 encodes.
     */
    private BreakCallBack rotateLeftImmediateMaskInsert() {
        return new BreakCallBack() {
            @Override
            public boolean pcodeCallback(PcodeOpRaw op) {
                MemoryState memory = emulate.getMemoryState();
                byte[] destination = readChunk(memory, op.getInput(1));
                byte[] source = readChunk(memory, op.getInput(2));
                int mask = (int) (memory.getValue(op.getInput(3)) & 0xF);
                int rotate = (int) (memory.getValue(op.getInput(4)) & 0x3);
                byte[] out = new byte[16];
                for (int element = 0; element < 4; ++element) {
                    boolean fromSource = ((mask >> (3 - element)) & 1) != 0;
                    if (fromSource) {
                        System.arraycopy(source, ((element + rotate) & 3) * 4, out,
                            element * 4, 4);
                    }
                    else {
                        System.arraycopy(destination, element * 4, out, element * 4, 4);
                    }
                }
                writeChunk(memory, op.getOutput(), out);
                countFired("vectorRotateLeftImmediateMaskInsert128");
                return true;
            }
        };
    }

    /**
     * Load Vector for Shift Left: sh = EA & 0xF, and the result is the byte
     * sequence sh, sh+1, ... sh+15. It reads NO memory -- it builds the control
     * vector `vperm` needs to gather one unaligned load out of two aligned ones,
     * which is the shape of the vectorised memcpy at 0x821F398C.
     */
    private BreakCallBack loadVectorForShiftLeft() {
        return new BreakCallBack() {
            @Override
            public boolean pcodeCallback(PcodeOpRaw op) {
                MemoryState memory = emulate.getMemoryState();
                long address = (baseRegisterValue(memory, op.getInput(1))
                    + memory.getValue(op.getInput(2))) & 0xffffffffL;
                int shift = (int) (address & 0xF);
                byte[] out = new byte[16];
                for (int index = 0; index < 16; ++index) {
                    out[index] = (byte) (shift + index);
                }
                writeChunk(memory, op.getOutput(), out);
                countFired("loadVectorForShiftLeft");
                return true;
            }
        };
    }

    // NO `vectorPermute` BEHAVIOUR IS SUPPLIED HERE, AND ONE WAS WRITTEN FIRST.
    //
    // `vperm` emits CALLOTHER<vectorPermute> and appears in the CALLOTHER census
    // beside the four above, so cycle 1320 implemented it. It never fired. The
    // emulator already implements it, from a layer neither SLEIGH nor this
    // harness: `ghidra.program.emulation.PPCEmulateInstructionStateModifier`
    // registers a `vectorPermuteOpBehavior` for exactly this one op, and that
    // registration wins over `registerCallOtherCallback`.
    //
    // Measured, not deduced: running 0x821F399C with `vmx` OFF -- no callback of
    // ours registered at all -- still returns the correct ISA permute.
    //
    // So the model was deleted rather than left in place. An asserted behaviour
    // that cannot fire is worse than none: the snapshot would carry it in
    // `asserted_semantics_enabled` while the value came from somewhere else.

    private ghidra.program.model.address.AddressSpace defaultSpace() {
        return currentProgram.getAddressFactory().getDefaultAddressSpace();
    }

    private void registerAssertedSemantics(EmulatorHelper emulator) {
        emulator.registerCallOtherCallback("vectorMergeHighWord",
            mergeWord("vectorMergeHighWord", 0));
        emulator.registerCallOtherCallback("vectorMergeLowWord",
            mergeWord("vectorMergeLowWord", 2));
        emulator.registerCallOtherCallback("loadVectorLeftIndexed128",
            loadVectorLeftIndexed());
        emulator.registerCallOtherCallback("vectorRotateLeftImmediateMaskInsert128",
            rotateLeftImmediateMaskInsert());
        emulator.registerCallOtherCallback("loadVectorForShiftLeft",
            loadVectorForShiftLeft());
    }

    /**
     * Every field a spec sets, cleared. Batch mode runs specs in one process,
     * so a case inheriting the previous case's region or stub would be a silent
     * wrong answer rather than a failure.
     */
    private void resetCase() {
        regions.clear();
        gprSeeds.clear();
        fprSeeds.clear();
        vecSeeds.clear();
        stubs.clear();
        captureGpr.clear();
        captureFpr.clear();
        captureVec.clear();
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
        vmxEnabled = false;
        hintOps.clear();
        hintsFired.clear();
        aliasEnabled = false;
        aliasCopies = 0;
        assertedFired.clear();
        overrides.clear();
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
                    else if ("bytes".equals(region.kind)) {
                        // A fixture small enough to read in the spec beats one in
                        // a file the reader has to go and open.
                        if ((argument.length() & 1) != 0) {
                            throw new IllegalArgumentException("odd hex length: " + argument);
                        }
                        region.contents = new byte[argument.length() / 2];
                        for (int i = 0; i < region.contents.length; ++i) {
                            region.contents[i] = (byte) Integer.parseInt(
                                argument.substring(i * 2, i * 2 + 2), 16);
                        }
                        region.size = region.contents.length;
                        region.sha256 = sha256(region.contents);
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
                case "vec":
                    vecSeeds.put(parts[1], parts[2]);
                    break;
                case "sp":
                    stackSpec = parts[1];
                    break;
                case "override":
                    overrides.put(Long.decode(parts[1]) & 0xffffffffL, parts[2]);
                    break;
                case "dump":
                    dumpRegions.add(parts[1]);
                    break;
                case "alias":
                    aliasEnabled = "on".equals(parts[1]);
                    break;
                case "hint":
                    hintOps.add(parts[1]);
                    break;
                case "vmx":
                    // Off unless a spec asks: a snapshot produced without it is
                    // retail instructions only, and that is the default worth
                    // having to opt out of.
                    vmxEnabled = "on".equals(parts[1]);
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
                        else if (what.startsWith("vec:")) {
                            captureVec.add(what.substring(4));
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
            byte[] sliceB = new byte[index - start];
            System.arraycopy(valuesB, start, sliceB, 0, sliceB.length);
            total += slice.length;
            // BOTH PASSES, because `after_hex` alone cannot describe a
            // read-modify-write. A byte the function OR-ed reads 0xCD|mask in
            // pass A, so every mask bit that 0xCD already carries -- bits 0, 2,
            // 3, 6 and 7 -- is invisible there and visible in pass B, where the
            // byte reads the mask itself. Cycle 1320 reported "record+0x0B has
            // bit 5 set and no other" from pass A alone and could not have seen
            // those five. `after_hex_b` is additive: the digest in
            // tools/emit_ac6_reader_digests.py is defined on `after_hex`.
            writes.add(String.format(
                "{\"address\": \"0x%08x\", \"size\": %d, \"after_hex\": \"%s\", "
                + "\"after_hex_b\": \"%s\"}",
                region.base + start, slice.length, hex(slice), hex(sliceB)));
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

            // Cleared per pass, not per case: both poison passes execute the
            // same instructions, so counting across them would report double.
            assertedFired.clear();
            hintsFired.clear();
            if (vmxEnabled) {
                registerAssertedSemantics(emulator);
            }
            for (String hint : hintOps) {
                emulator.registerCallOtherCallback(hint, noOperation(hint));
            }

            for (Region region : regions.values()) {
                if (region.contents != null) {
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
            for (Map.Entry<String, String> seed : vecSeeds.entrySet()) {
                emulator.writeRegister(registerName(seed.getKey(),
                    seed.getKey().toUpperCase()),
                    new BigInteger(seed.getValue().trim(), 16));
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
                String override = overrides.get(here);
                if (override != null) {
                    applyOverride(override, here, emulator);
                    if (aliasEnabled) {
                        mirrorWrites(getInstructionAt(toAddr(here)), emulator);
                    }
                    emulator.writeRegister(pc, here + 4);
                    steps++;
                    continue;
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
                ghidra.program.model.listing.Instruction executed =
                    aliasEnabled ? getInstructionAt(toAddr(here)) : null;
                if (!emulator.step(monitor)) {
                    exitKind = "fault";
                    exitDetail = String.valueOf(emulator.getLastError());
                    break;
                }
                if (aliasEnabled) {
                    mirrorWrites(executed, emulator);
                }
                steps++;
            }
            if (steps >= maxSteps) {
                exitKind = "step_limit";
            }

            Map<String, byte[]> result = new LinkedHashMap<>();
            for (Region region : regions.values()) {
                if ("poison".equals(region.kind) || dumpRegions.contains(region.name)) {
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
                for (String name : captureVec) {
                    // Sixteen bytes, zero-padded: a BigInteger drops leading
                    // zeros and a vector whose top word is zero would then
                    // compare unequal to itself written differently.
                    String resolved = registerName(name, name.toUpperCase());
                    String bits = emulator.readRegister(resolved)
                        .and(new BigInteger("ff".repeat(16), 16)).toString(16);
                    capturedValues.put(name,
                        "0x" + "0".repeat(32 - bits.length()) + bits);
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

        List<String> dumps = new ArrayList<>();
        for (String name : dumpRegions) {
            Region region = regions.get(name);
            if (region == null) {
                throw new IllegalArgumentException("dump names no region: " + name);
            }
            dumps.add(String.format(
                "{\"name\": \"%s\", \"base\": \"0x%08x\", \"size\": %d, "
                + "\"after_hex\": \"%s\", \"after_hex_b\": \"%s\"}",
                region.name, region.base, region.size,
                hex(passA.get(region.name)), hex(passB.get(region.name))));
        }

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
        // Deliberately loud. A snapshot that leaned on a hand-written
        // instruction model is not the same claim as one that did not, and the
        // difference has to survive being read quickly.
        json.append(String.format("    \"asserted_semantics_enabled\": %s,%n", vmxEnabled));
        json.append("    \"hint_noops\": {");
        boolean firstHint = true;
        for (Map.Entry<String, Integer> entry : hintsFired.entrySet()) {
            json.append(firstHint ? "\n      " : ",\n      ");
            firstHint = false;
            json.append(String.format("\"%s\": %d", entry.getKey(), entry.getValue()));
        }
        json.append(hintsFired.isEmpty() ? "}," : "\n    },").append("\n");
        json.append(String.format("    \"register_file_bridge\": %s,%n", aliasEnabled));
        json.append(String.format("    \"alias_copies\": %d,%n", aliasCopies));
        json.append("    \"asserted_semantics\": {");
        boolean firstOp = true;
        for (Map.Entry<String, Integer> entry : assertedFired.entrySet()) {
            json.append(firstOp ? "\n      " : ",\n      ");
            firstOp = false;
            json.append(String.format("\"%s\": %d", entry.getKey(), entry.getValue()));
        }
        json.append(assertedFired.isEmpty() ? "}," : "\n    },").append("\n");
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
        json.append(writes.isEmpty() ? "]," : "\n  ],").append("\n");
        json.append("  \"region_dumps\": [");
        for (int index = 0; index < dumps.size(); ++index) {
            json.append(index == 0 ? "\n    " : ",\n    ").append(dumps.get(index));
        }
        json.append(dumps.isEmpty() ? "]" : "\n  ]").append("\n");
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
