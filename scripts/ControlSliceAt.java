// Emit branch conditions whose successor choice is required to reach a callsite.
// Usage: ControlSliceAt.java FUNCTION CALLSITE OUT_TXT
// Read-only; one bounded function CFG.
// @category AC6

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.PcodeBlock;
import ghidra.program.model.pcode.PcodeBlockBasic;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.Varnode;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

public class ControlSliceAt extends GhidraScript {
    private String expression(Varnode node, Set<Varnode> seen, int depth) {
        if (node == null) return "-";
        if (node.isConstant()) return String.format("0x%x", node.getOffset());
        if (depth > 20 || !seen.add(node)) return node.toString();
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

    private boolean reaches(PcodeBlockBasic from, PcodeBlockBasic target,
            Set<PcodeBlockBasic> seen) {
        if (from.equals(target)) return true;
        if (!seen.add(from)) return false;
        for (int i = 0; i < from.getOutSize(); ++i) {
            if (reaches((PcodeBlockBasic) from.getOut(i), target, seen)) return true;
        }
        return false;
    }

    private PcodeOp lastOperation(PcodeBlockBasic block) {
        PcodeOp last = null;
        Iterator<PcodeOp> operations = block.getIterator();
        while (operations.hasNext()) last = operations.next();
        return last;
    }

    private Map<PcodeBlockBasic, Set<PcodeBlockBasic>> postdominators(
            List<PcodeBlockBasic> blocks) {
        Set<PcodeBlockBasic> all = new HashSet<PcodeBlockBasic>(blocks);
        Map<PcodeBlockBasic, Set<PcodeBlockBasic>> result =
            new HashMap<PcodeBlockBasic, Set<PcodeBlockBasic>>();
        for (PcodeBlockBasic block : blocks) {
            Set<PcodeBlockBasic> initial = new HashSet<PcodeBlockBasic>();
            if (block.getOutSize() == 0) initial.add(block);
            else initial.addAll(all);
            result.put(block, initial);
        }
        boolean changed;
        do {
            changed = false;
            for (PcodeBlockBasic block : blocks) {
                if (block.getOutSize() == 0) continue;
                Set<PcodeBlockBasic> next = null;
                for (int i = 0; i < block.getOutSize(); ++i) {
                    Set<PcodeBlockBasic> successor = result.get(
                        (PcodeBlockBasic) block.getOut(i));
                    if (next == null) next = new HashSet<PcodeBlockBasic>(successor);
                    else next.retainAll(successor);
                }
                next.add(block);
                if (!next.equals(result.get(block))) {
                    result.put(block, next);
                    changed = true;
                }
            }
        } while (changed);
        return result;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 3) {
            throw new IllegalArgumentException(
                "usage: ControlSliceAt FUNCTION CALLSITE OUT_TXT");
        }
        Address functionAddress = toAddr(args[0]);
        Address callsite = toAddr(args[1]);
        Function function = currentProgram.getFunctionManager()
            .getFunctionAt(functionAddress);
        if (function == null) throw new IllegalArgumentException("missing function");

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try {
            DecompileResults decompiled = decompiler.decompileFunction(function, 120, monitor);
            if (!decompiled.decompileCompleted()) {
                throw new IllegalStateException(decompiled.getErrorMessage());
            }
            HighFunction high = decompiled.getHighFunction();
            List<PcodeBlockBasic> blocks = new ArrayList<PcodeBlockBasic>(
                high.getBasicBlocks());
            PcodeBlockBasic target = null;
            for (PcodeBlockBasic block : blocks) {
                if (block.getStart().compareTo(callsite) <= 0 &&
                        block.getStop().compareTo(callsite) >= 0) {
                    target = block;
                    break;
                }
            }
            if (target == null) throw new IllegalStateException("callsite block missing");
            Map<PcodeBlockBasic, Set<PcodeBlockBasic>> postdom = postdominators(blocks);

            try (PrintWriter out = new PrintWriter(args[2])) {
                out.println("QUALIFICATION project=ace-combat-6-demo program=Default.xex mode=readOnly/noanalysis");
                out.printf("TARGET function=%s callsite=%s block=%s..%s blocks=%d%n",
                    function.getEntryPoint(), callsite, target.getStart(), target.getStop(), blocks.size());
                int guards = 0;
                for (PcodeBlockBasic block : blocks) {
                    if (block.getOutSize() < 2 ||
                            !reaches(block, target, new HashSet<PcodeBlockBasic>())) continue;
                    int reaching = 0;
                    for (int i = 0; i < block.getOutSize(); ++i) {
                        if (reaches((PcodeBlockBasic) block.getOut(i), target,
                                new HashSet<PcodeBlockBasic>())) ++reaching;
                    }
                    if (reaching == 0 || reaching == block.getOutSize()) continue;
                    PcodeOp branch = lastOperation(block);
                    Varnode condition = branch != null && branch.getOpcode() == PcodeOp.CBRANCH
                        ? branch.getInput(1) : null;
                    out.printf("GUARD block=%s..%s pc=%s target_postdominates=%s condition=%s%n",
                        block.getStart(), block.getStop(),
                        branch == null ? "-" : branch.getSeqnum().getTarget(),
                        postdom.get(block).contains(target), expression(condition));
                    for (int i = 0; i < block.getOutSize(); ++i) {
                        PcodeBlock successor = block.getOut(i);
                        out.printf("  EDGE to=%s..%s reaches_target=%s%n",
                            successor.getStart(), successor.getStop(),
                            reaches((PcodeBlockBasic) successor, target,
                                new HashSet<PcodeBlockBasic>()));
                    }
                    ++guards;
                }
                out.printf("SUMMARY guards=%d%n", guards);
            }
        } finally {
            decompiler.dispose();
        }
        println("AC6_CONTROL_SLICE out=" + args[2]);
    }
}
