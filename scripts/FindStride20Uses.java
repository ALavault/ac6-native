// Find every site that scales an index by 20 (0x14), the stride of the mission
// counter table, and report whether its function also loads a given
// displacement from a pointer - by default 0x5C, where the loader stores that
// table.
//
// The compiler emits the multiply as either `mulli rD,rS,0x14` or, more often,
// the shift/add form the tag-7 reader uses:
//
//     rlwinm rA,rX,2,0,0x1d      ; rA = index * 4
//     add    rB,rX,rA            ; rB = index * 5
//     rlwinm rB,rB,2,0,0x1d      ; rB = index * 20
//
// Both are recognised. Read-only.
//
// usage: FindStride20Uses.java [DISPLACEMENT]
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;

import java.util.ArrayList;
import java.util.List;

public class FindStride20Uses extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        long displacement = args.length > 0 ? Long.decode(args[0]) : 0x5C;

        int functions = 0;
        int sites = 0;
        for (Function function : currentProgram.getFunctionManager().getFunctions(true)) {
            if (monitor.isCancelled()) {
                return;
            }
            List<Instruction> body = new ArrayList<>();
            for (Instruction instruction : currentProgram.getListing()
                    .getInstructions(function.getBody(), true)) {
                body.add(instruction);
            }

            List<String> scaled = new ArrayList<>();
            for (int index = 0; index < body.size(); index++) {
                if (isMultiplyBy20(body, index)) {
                    scaled.add(body.get(index).getAddress().toString());
                }
            }
            if (scaled.isEmpty()) {
                continue;
            }
            List<String> loads = new ArrayList<>();
            for (Instruction instruction : body) {
                if (!instruction.getMnemonicString().equals("lwz")) {
                    continue;
                }
                Scalar scalar = displacementOf(instruction);
                if (scalar != null && scalar.getSignedValue() == displacement) {
                    loads.add(instruction.getAddress().toString());
                }
            }
            functions++;
            sites += scaled.size();
            println(String.format(
                    "AC6_STRIDE20 function=%s scaled=%s displacement_%x_loads=%s",
                    function.getEntryPoint(), String.join(",", scaled),
                    displacement, loads.isEmpty() ? "none" : String.join(",", loads)));
        }
        println("AC6_STRIDE20_TOTAL functions=" + functions + " sites=" + sites);
    }

    private boolean isMultiplyBy20(List<Instruction> body, int index) {
        Instruction instruction = body.get(index);
        if (instruction.getMnemonicString().equals("mulli")) {
            Scalar scalar = scalarOf(instruction, 2);
            return scalar != null && scalar.getUnsignedValue() == 0x14;
        }
        // rlwinm rA,rX,2 ; add rB,rX,rA ; rlwinm rB,rB,2
        if (!isShiftLeft2(instruction)) {
            return false;
        }
        Register source = registerOf(instruction, 1);
        Register quadruple = registerOf(instruction, 0);
        if (source == null || quadruple == null) {
            return false;
        }
        for (int step = index + 1; step < body.size() && step <= index + 4; step++) {
            Instruction add = body.get(step);
            if (!add.getMnemonicString().equals("add")) {
                continue;
            }
            Register left = registerOf(add, 1);
            Register right = registerOf(add, 2);
            boolean matches = (source.equals(left) && quadruple.equals(right))
                    || (source.equals(right) && quadruple.equals(left));
            if (!matches) {
                continue;
            }
            Register sum = registerOf(add, 0);
            for (int tail = step + 1; tail < body.size() && tail <= step + 4; tail++) {
                Instruction shift = body.get(tail);
                if (isShiftLeft2(shift) && sum != null
                        && sum.equals(registerOf(shift, 1))) {
                    return true;
                }
            }
        }
        return false;
    }

    private boolean isShiftLeft2(Instruction instruction) {
        if (!instruction.getMnemonicString().equals("rlwinm")) {
            return false;
        }
        Scalar shift = scalarOf(instruction, 2);
        Scalar begin = scalarOf(instruction, 3);
        Scalar end = scalarOf(instruction, 4);
        return shift != null && begin != null && end != null
                && shift.getUnsignedValue() == 2 && begin.getUnsignedValue() == 0
                && end.getUnsignedValue() == 0x1D;
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
        Object[] objects = instruction.getOpObjects(1);
        for (Object object : objects) {
            if (object instanceof Scalar) {
                return (Scalar) object;
            }
        }
        return null;
    }
}
