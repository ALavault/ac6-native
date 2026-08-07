// Report the second hop of a two-load chain.
//
// A consumer that reaches a field through a stored pointer emits
//
//     lwz rX, BASE(rY)        ; rX = the pointer field
//     ...
//     lwz rZ, CHILD(rX)       ; rZ = the field inside it
//
// Searching for BASE alone is hopeless when BASE is a common displacement.
// This script anchors on BASE, then reports every displacement loaded from the
// register it produced, within a small instruction window and only while that
// register is still live. Read-only.
//
// usage: FindChainedLoad.java BASE_DISPLACEMENT [WINDOW]
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;

import java.util.ArrayList;
import java.util.List;

public class FindChainedLoad extends GhidraScript {

    private static final List<String> LOADS =
            List.of("lwz", "lbz", "lhz", "lha", "lwzu", "lfs", "lfd");

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            printerr("usage: FindChainedLoad.java BASE_DISPLACEMENT [WINDOW]");
            return;
        }
        long base = Long.decode(args[0]);
        int window = args.length > 1 ? Integer.decode(args[1]) : 12;

        for (Function function : currentProgram.getFunctionManager().getFunctions(true)) {
            if (monitor.isCancelled()) {
                return;
            }
            List<Instruction> body = new ArrayList<>();
            for (Instruction instruction : currentProgram.getListing()
                    .getInstructions(function.getBody(), true)) {
                body.add(instruction);
            }
            for (int index = 0; index < body.size(); index++) {
                Instruction anchor = body.get(index);
                if (!anchor.getMnemonicString().equals("lwz")) {
                    continue;
                }
                Scalar displacement = displacement(anchor);
                if (displacement == null || displacement.getSignedValue() != base) {
                    continue;
                }
                Register produced = destination(anchor);
                if (produced == null) {
                    continue;
                }
                for (int step = index + 1;
                        step < body.size() && step <= index + window; step++) {
                    Instruction follower = body.get(step);
                    if (LOADS.contains(follower.getMnemonicString())) {
                        Register followerBase = memoryBase(follower);
                        Scalar followerDisplacement = displacement(follower);
                        if (followerBase != null && followerDisplacement != null
                                && followerBase.equals(produced)) {
                            println(String.format(
                                    "AC6_CHAIN function=%s base_at=%s child_at=%s child_disp=0x%X %s",
                                    function.getEntryPoint(), anchor.getAddress(),
                                    follower.getAddress(),
                                    followerDisplacement.getSignedValue(), follower));
                        }
                    }
                    if (writes(follower, produced)) {
                        break;
                    }
                }
            }
        }
    }

    private boolean writes(Instruction instruction, Register register) {
        for (Object result : instruction.getResultObjects()) {
            if (result instanceof Register && result.equals(register)) {
                return true;
            }
        }
        return false;
    }

    private Register destination(Instruction instruction) {
        Object[] objects = instruction.getOpObjects(0);
        if (objects.length == 1 && objects[0] instanceof Register) {
            return (Register) objects[0];
        }
        return null;
    }

    private Scalar displacement(Instruction instruction) {
        Object[] objects = instruction.getOpObjects(1);
        for (Object object : objects) {
            if (object instanceof Scalar) {
                return (Scalar) object;
            }
        }
        return null;
    }

    private Register memoryBase(Instruction instruction) {
        Object[] objects = instruction.getOpObjects(1);
        for (Object object : objects) {
            if (object instanceof Register) {
                return (Register) object;
            }
        }
        return null;
    }
}
