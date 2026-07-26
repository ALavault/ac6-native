// Inspect vtable slots whose target function contains an operand with a field
// displacement of interest. Static candidate finder; no liveness inference.
// Usage: InspectVtableFieldAccess.java <vtable> <word-count> <scalar>
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;

public class InspectVtableFieldAccess extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 3) {
            throw new IllegalArgumentException(
                "usage: InspectVtableFieldAccess <vtable> <word-count> <scalar>");
        }
        Address vtable = toAddr(Long.decode(args[0]));
        int words = Integer.decode(args[1]);
        long expected = Long.decode(args[2]);
        for (int index = 0; index < words; ++index) {
            Address slot = vtable.add(index * 4L);
            long raw = currentProgram.getMemory().getInt(slot) & 0xffffffffL;
            Address target = toAddr(raw);
            Function function = currentProgram.getFunctionManager().getFunctionAt(target);
            if (function == null) {
                function = currentProgram.getFunctionManager().getFunctionContaining(target);
            }
            if (function == null) {
                println(String.format("slot=+0x%x target=%s no-function",
                    index * 4L, target));
                continue;
            }
            for (Instruction instruction : currentProgram.getListing()
                    .getInstructions(function.getBody(), true)) {
                boolean match = false;
                for (int operand = 0; operand < instruction.getNumOperands(); ++operand) {
                    for (Object object : instruction.getOpObjects(operand)) {
                        if (object instanceof Scalar
                                && ((Scalar)object).getUnsignedValue() == expected) {
                            match = true;
                        }
                    }
                }
                if (match) {
                    println(String.format("slot=+0x%x target=%s function=%s %s",
                        index * 4L, target, function.getName(), instruction));
                }
            }
        }
    }
}
