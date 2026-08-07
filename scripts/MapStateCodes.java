// Extract, per state handler, the code it publishes and the substate it enters.
//
// Reading one handler settled the idiom. On the entry signal a state writes a
// small integer to this+0x260 - the field mission_manager_update tests - and on
// the initial-transition signal it writes another handler's address to
// this+0x350, the state field the transition engine maintains. So each state
// carries a number the game itself uses, and names its default substate.
//
// Both are recovered by tracking the stored register back to the instruction
// that produced it: `li rX, N` for the code, and a `lis`/`ori` or `lis`/`addi`
// pair for the address. Read-only.
//
// usage: MapStateCodes.java LOW HIGH
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;

import java.util.ArrayList;
import java.util.List;
import java.util.TreeSet;

public class MapStateCodes extends GhidraScript {

    private static final long STATE_CODE_FIELD = 0x260;
    private static final long STATE_POINTER_FIELD = 0x350;

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            printerr("usage: MapStateCodes.java LOW HIGH");
            return;
        }
        long low = Long.decode(args[0]);
        long high = Long.decode(args[1]);

        for (Function function : currentProgram.getFunctionManager().getFunctions(true)) {
            long entry = function.getEntryPoint().getOffset();
            if (entry < low || entry >= high) {
                continue;
            }
            List<Instruction> body = new ArrayList<>();
            for (Instruction instruction : currentProgram.getListing()
                    .getInstructions(function.getBody(), true)) {
                body.add(instruction);
            }

            TreeSet<String> codes = new TreeSet<>();
            TreeSet<String> substates = new TreeSet<>();
            for (int index = 0; index < body.size(); index++) {
                Instruction instruction = body.get(index);
                if (!instruction.getMnemonicString().equals("stw")) {
                    continue;
                }
                Scalar displacement = displacementOf(instruction);
                Register source = registerOf(instruction, 0);
                if (displacement == null || source == null) {
                    continue;
                }
                long field = displacement.getSignedValue();
                if (field == STATE_CODE_FIELD) {
                    Long value = literalBefore(body, index, source);
                    codes.add(value == null ? "?" : Long.toString(value));
                } else if (field == STATE_POINTER_FIELD) {
                    Long value = addressBefore(body, index, source);
                    if (value != null) {
                        substates.add(String.format("%08x", value));
                    }
                }
            }
            String superstate = superstateOf(body, low, high);
            if (codes.isEmpty() && substates.isEmpty() && superstate == null) {
                continue;
            }
            println(String.format("AC6_STATE %s code=[%s] initial=[%s] super=%s",
                    function.getEntryPoint(), String.join(",", codes),
                    String.join(",", substates),
                    superstate == null ? "none" : superstate));
        }
    }

    /**
     * The superstate: the address a handler materialises on its fall-through
     * path, after the chain of signal comparisons at the top has all missed.
     * That is where this idiom returns the parent state, and reading it there
     * rather than "last in the body" is what keeps the relation acyclic.
     */
    private String superstateOf(List<Instruction> body, long low, long high) {
        int dispatchEnd = -1;
        for (int index = 0; index + 1 < body.size() && index < 40; index++) {
            Instruction compare = body.get(index);
            if (!compare.getMnemonicString().startsWith("cmp")) {
                continue;
            }
            Scalar value = scalarOf(compare, 2);
            if (value == null) {
                value = scalarOf(compare, 1);
            }
            if (value == null) {
                continue;
            }
            long signal = value.getSignedValue();
            if (signal > -1 || signal < -5) {
                continue;
            }
            Instruction branch = body.get(index + 1);
            if (branch.getMnemonicString().startsWith("b")) {
                dispatchEnd = Math.max(dispatchEnd, index + 1);
            }
        }
        if (dispatchEnd < 0) {
            return null;
        }
        for (int index = dispatchEnd + 1; index < body.size() && index <= dispatchEnd + 24;
                index++) {
            Instruction instruction = body.get(index);
            String mnemonic = instruction.getMnemonicString();
            if (!mnemonic.equals("ori") && !mnemonic.equals("addi")
                    && !mnemonic.equals("subi")) {
                continue;
            }
            Register destination = registerOf(instruction, 0);
            if (destination == null) {
                continue;
            }
            Long address = addressBefore(body, index + 1, destination);
            if (address != null && address >= low && address < high) {
                return String.format("%08x", address);
            }
        }
        return null;
    }

    /** The `li rX, N` that most recently produced this register. */
    private Long literalBefore(List<Instruction> body, int index, Register source) {
        for (int step = index - 1; step >= 0 && step >= index - 24; step--) {
            Instruction candidate = body.get(step);
            Register destination = registerOf(candidate, 0);
            if (destination == null || !destination.equals(source)) {
                continue;
            }
            if (candidate.getMnemonicString().equals("li")) {
                Scalar value = scalarOf(candidate, 1);
                return value == null ? null : value.getSignedValue();
            }
            return null;  // the register was produced some other way
        }
        return null;
    }

    /** The `lis`+`ori`/`addi` pair that most recently produced this register. */
    private Long addressBefore(List<Instruction> body, int index, Register source) {
        for (int step = index - 1; step >= 0 && step >= index - 24; step--) {
            Instruction candidate = body.get(step);
            Register destination = registerOf(candidate, 0);
            if (destination == null || !destination.equals(source)) {
                continue;
            }
            String mnemonic = candidate.getMnemonicString();
            if (!mnemonic.equals("ori") && !mnemonic.equals("addi")
                    && !mnemonic.equals("subi")) {
                return null;
            }
            Register base = registerOf(candidate, 1);
            Scalar low = scalarOf(candidate, 2);
            if (base == null || low == null) {
                return null;
            }
            for (int back = step - 1; back >= 0 && back >= step - 24; back--) {
                Instruction high = body.get(back);
                Register highDestination = registerOf(high, 0);
                if (highDestination == null || !highDestination.equals(base)) {
                    continue;
                }
                if (!high.getMnemonicString().equals("lis")) {
                    return null;
                }
                Scalar upper = scalarOf(high, 1);
                if (upper == null) {
                    return null;
                }
                long value = (upper.getUnsignedValue() & 0xFFFF) << 16;
                if (mnemonic.equals("ori")) {
                    value += low.getUnsignedValue();
                } else if (mnemonic.equals("subi")) {
                    value -= low.getUnsignedValue();
                } else {
                    value += (short) low.getUnsignedValue();
                }
                return value;
            }
            return null;
        }
        return null;
    }

    private Register registerOf(Instruction instruction, int operand) {
        Object[] objects = instruction.getOpObjects(operand);
        if (objects.length == 1 && objects[0] instanceof Register) {
            return (Register) objects[0];
        }
        return null;
    }

    private Scalar scalarOf(Instruction instruction, int operand) {
        if (operand >= instruction.getNumOperands()) {
            return null;
        }
        Object[] objects = instruction.getOpObjects(operand);
        if (objects.length == 1 && objects[0] instanceof Scalar) {
            return (Scalar) objects[0];
        }
        return null;
    }

    private Scalar displacementOf(Instruction instruction) {
        for (Object object : instruction.getOpObjects(1)) {
            if (object instanceof Scalar) {
                return (Scalar) object;
            }
        }
        return null;
    }
}
