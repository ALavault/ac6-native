// Classify high-p-code LOAD/STORE addresses that depend on both a global
// pointer location and a field offset. The decompiler SSA supplies CFG joins.
// Usage: ClassifyGlobalIndexedAccesses.java OUT GLOBAL OFFSET SITE...
// Read-only. Three known PPC sites calibrate inclusion and exclusion.
// @category AC6.Evidence

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.PcodeOpAST;
import ghidra.program.model.pcode.Varnode;
import java.io.PrintWriter;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.TreeMap;
import java.util.TreeSet;
import java.util.Set;

public class ClassifyGlobalIndexedAccesses extends GhidraScript {
    private boolean contains(Varnode node, long value, Set<Varnode> seen, int depth) {
        if (node == null || depth > 32 || !seen.add(node)) return false;
        if ((node.isConstant() || node.isAddress()) && node.getOffset() == value) return true;
        PcodeOp definition = node.getDef();
        if (definition == null) return false;
        for (int i = 0; i < definition.getNumInputs(); ++i) {
            if (contains(definition.getInput(i), value, seen, depth + 1)) return true;
        }
        return false;
    }

    private boolean contains(Varnode node, long value) {
        return contains(node, value, new HashSet<Varnode>(), 0);
    }

    private String expression(Varnode node, Set<Varnode> seen, int depth) {
        if (node == null) return "-";
        if (node.isConstant()) return String.format("0x%x", node.getOffset());
        if (depth > 14 || !seen.add(node)) return node.toString();
        PcodeOp definition = node.getDef();
        if (definition == null) return node.toString();
        StringBuilder result = new StringBuilder(definition.getMnemonic()).append('(');
        for (int i = 0; i < definition.getNumInputs(); ++i) {
            if (i != 0) result.append(',');
            result.append(expression(definition.getInput(i), seen, depth + 1));
        }
        return result.append(')').toString();
    }

    private String expression(Varnode node) {
        return expression(node, new HashSet<Varnode>(), 0);
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 4) {
            throw new IllegalArgumentException(
                "usage: ClassifyGlobalIndexedAccesses OUT GLOBAL OFFSET SITE...");
        }
        long global = Long.decode(args[1]) & 0xffffffffL;
        long offset = Long.decode(args[2]) & 0xffffffffL;
        Set<Address> wantedSites = new TreeSet<Address>();
        Set<Function> functions = new HashSet<Function>();
        for (int i = 3; i < args.length; ++i) {
            Address site = toAddr(Long.parseUnsignedLong(
                args[i].replaceFirst("^(?i)0x", ""), 16) & 0xffffffffL);
            wantedSites.add(site);
            Function function = currentProgram.getFunctionManager().getFunctionContaining(site);
            if (function != null) functions.add(function);
        }
        Set<Address> observedSites = new HashSet<Address>();
        Map<Address, String> qualifiedSites = new TreeMap<Address, String>();
        boolean consumer = false;
        boolean writer = false;
        boolean constructor = false;
        int loads = 0;
        int stores = 0;
        int failures = 0;

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (PrintWriter out = new PrintWriter(args[0])) {
            out.println("QUALIFICATION project=ace-combat-6-demo program=Default.xex mode=readOnly/noanalysis");
            for (Function function : functions) {
                DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
                if (!result.decompileCompleted()) {
                    ++failures;
                    out.printf("DECOMPILE_FAILED function=%s error=%s%n",
                        function.getEntryPoint(), result.getErrorMessage());
                    continue;
                }
                HighFunction high = result.getHighFunction();
                Iterator<PcodeOpAST> operations = high.getPcodeOps();
                while (operations.hasNext()) {
                    PcodeOpAST operation = operations.next();
                    int opcode = operation.getOpcode();
                    if (opcode != PcodeOp.LOAD && opcode != PcodeOp.STORE) continue;
                    Address pc = operation.getSeqnum().getTarget();
                    if (!wantedSites.contains(pc)) continue;
                    observedSites.add(pc);
                    Varnode address = operation.getInput(1);
                    if (!contains(address, global) || !contains(address, offset)) continue;
                    String kind = opcode == PcodeOp.LOAD ? "LOAD" : "STORE";
                    qualifiedSites.put(pc, String.format(
                        "QUALIFIED %s function=%s pc=%s address=%s value=%s",
                        kind, function.getEntryPoint(), pc, expression(address),
                        opcode == PcodeOp.STORE ? expression(operation.getInput(2)) : "-"));
                }
            }
            int rejected = 0;
            int missing = 0;
            for (Address site : wantedSites) {
                String line = qualifiedSites.get(site);
                if (line != null) {
                    out.println(line);
                    if (line.startsWith("QUALIFIED LOAD")) ++loads; else ++stores;
                } else if (observedSites.contains(site)) {
                    out.printf("REJECTED pc=%s%n", site);
                    ++rejected;
                } else {
                    out.printf("MISSING pc=%s%n", site);
                    ++missing;
                }
            }
            consumer = qualifiedSites.containsKey(toAddr(0x82220640L));
            writer = qualifiedSites.containsKey(toAddr(0x82212e54L));
            constructor = qualifiedSites.containsKey(toAddr(0x821e0b7cL));
            out.printf("CALIBRATION consumer=%s writer=%s constructor=%s%n",
                consumer, writer, constructor);
            out.printf("SUMMARY requested=%d qualified=%d rejected=%d missing=%d loads=%d stores=%d failures=%d%n",
                wantedSites.size(), qualifiedSites.size(), rejected, missing, loads, stores, failures);
            if (missing != 0 || failures != 0) {
                throw new IllegalStateException("site classification incomplete");
            }
        } finally {
            decompiler.dispose();
        }
        if (!consumer || !writer || constructor) {
            throw new IllegalStateException("global indexed access calibration failed");
        }
        println("AC6_GLOBAL_INDEXED_ACCESSES out=" + args[0]);
    }
}
