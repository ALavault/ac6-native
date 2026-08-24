// Find high-p-code STORE addresses containing both a scaled index and field.
// Usage: FindScaledStoreWriters.java OUT_TXT
// Read-only. The known 0x820FF710 writer is a mandatory calibration witness.
// @category AC6

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.PcodeOpAST;
import ghidra.program.model.pcode.Varnode;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import java.io.PrintWriter;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

public class FindScaledStoreWriters extends GhidraScript {
    private DecompInterface decompiler;

    private boolean contains(Varnode node, long value, Set<Varnode> seen, int depth) {
        if (node == null || depth > 24 || !seen.add(node)) return false;
        if (node.isConstant() && node.getOffset() == value) return true;
        PcodeOp def = node.getDef();
        if (def == null) return false;
        for (int i = 0; i < def.getNumInputs(); ++i) {
            if (contains(def.getInput(i), value, seen, depth + 1)) return true;
        }
        return false;
    }

    private boolean contains(Varnode node, long value) {
        return contains(node, value, new HashSet<Varnode>(), 0);
    }

    private String expression(Varnode node, Set<Varnode> seen, int depth) {
        if (node == null) return "-";
        if (node.isConstant()) return String.format("0x%x", node.getOffset());
        if (depth > 12 || !seen.add(node)) return node.toString();
        PcodeOp def = node.getDef();
        if (def == null) return node.toString();
        StringBuilder out = new StringBuilder(def.getMnemonic()).append('(');
        for (int i = 0; i < def.getNumInputs(); ++i) {
            if (i != 0) out.append(',');
            out.append(expression(def.getInput(i), seen, depth + 1));
        }
        return out.append(')').toString();
    }

    private String expression(Varnode node) {
        return expression(node, new HashSet<Varnode>(), 0);
    }

    private HighFunction high(Function function) {
        DecompileResults result = decompiler.decompileFunction(function, 30, monitor);
        return result.decompileCompleted() ? result.getHighFunction() : null;
    }

    private int emitStores(Function function, PrintWriter out, boolean diagnostics) {
        HighFunction high = high(function);
        if (high == null) return 0;
        int matches = 0;
        Iterator<PcodeOpAST> operations = high.getPcodeOps();
        while (operations.hasNext()) {
            PcodeOpAST operation = operations.next();
            if (operation.getOpcode() != PcodeOp.STORE) continue;
            Varnode address = operation.getInput(1);
            boolean match = contains(address, 0x60) && contains(address, 0x110);
            if (!match && !diagnostics) continue;
            Address pc = operation.getSeqnum().getTarget();
            out.printf("STORE function=%s pc=%s match=%s address=%s value=%s%n",
                function.getEntryPoint(), pc, match,
                expression(address), expression(operation.getInput(2)));
            if (match) ++matches;
        }
        return matches;
    }

    private void emitInbound(Function function, PrintWriter out) {
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(function.getEntryPoint());
        while (references.hasNext()) {
            Reference reference = references.next();
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            out.printf("INBOUND function=%s from=%s caller=%s type=%s%n",
                function.getEntryPoint(), reference.getFromAddress(),
                caller == null ? "<no-function>" : caller.getEntryPoint(),
                reference.getReferenceType());
        }
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException("usage: FindScaledStoreWriters OUT_TXT");
        }

        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (PrintWriter out = new PrintWriter(args[0])) {
            out.println("QUALIFICATION project=ace-combat-6-demo program=Default.xex mode=readOnly/noanalysis");
            Function witness = currentProgram.getFunctionManager()
                .getFunctionContaining(toAddr(0x820ff710L));
            int witnessMatches = witness == null ? 0 : emitStores(witness, out, false);
            out.printf("CALIBRATION function=820ff710 matches=%d%n", witnessMatches);
            out.flush();
            if (witnessMatches == 0) {
                if (witness != null) emitStores(witness, out, true);
                out.flush();
                throw new IllegalStateException("calibration witness 0x820FF710 not matched");
            }

            int scanned = 0;
            int functions = 0;
            int matches = 0;
            FunctionIterator iterator = currentProgram.getFunctionManager().getFunctions(true);
            while (iterator.hasNext() && !monitor.isCancelled()) {
                Function function = iterator.next();
                if (function.equals(witness)) continue;
                ++scanned;
                int found = emitStores(function, out, false);
                if (found != 0) {
                    ++functions;
                    matches += found;
                    emitInbound(function, out);
                }
                if (scanned % 250 == 0) {
                    out.printf("PROGRESS scanned=%d last=%s%n", scanned, function.getEntryPoint());
                    out.flush();
                }
            }
            emitInbound(witness, out);
            out.printf("SUMMARY scanned=%d other_functions=%d other_stores=%d%n",
                scanned, functions, matches);
        } finally {
            decompiler.dispose();
        }
        println("AC6_SCALED_STORE_WRITERS out=" + args[0]);
    }
}
