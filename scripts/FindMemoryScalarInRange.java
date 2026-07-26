import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;

/**
 * Lists instructions in an address range that use an exact scalar operand.
 * This is deliberately a read-only candidate finder: a displacement such as
 * 0x30 is not promoted to an object field without receiver/vtable evidence.
 *
 * Usage: FindMemoryScalarInRange.java <start> <end> <scalar>
 */
public class FindMemoryScalarInRange extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 3) {
            throw new IllegalArgumentException(
                "usage: FindMemoryScalarInRange <start> <end> <scalar>");
        }

        Address start = toAddr(Long.decode(args[0]));
        Address end = toAddr(Long.decode(args[1]));
        long expected = Long.decode(args[2]);

        for (Instruction instruction : currentProgram.getListing()
                .getInstructions(start, true)) {
            if (instruction.getAddress().compareTo(end) >= 0) {
                break;
            }

            boolean match = false;
            for (int operand = 0; operand < instruction.getNumOperands(); ++operand) {
                for (Object object : instruction.getOpObjects(operand)) {
                    if (object instanceof Scalar
                            && ((Scalar) object).getUnsignedValue() == expected) {
                        match = true;
                        break;
                    }
                }
                if (match) {
                    break;
                }
            }

            if (!match) {
                continue;
            }

            Function function = currentProgram.getFunctionManager()
                .getFunctionContaining(instruction.getAddress());
            String functionName = function == null
                ? "<no-function>"
                : function.getEntryPoint().toString() + " " + function.getName();
            println(instruction.getAddress() + " " + functionName + " "
                + instruction);
        }
    }
}
