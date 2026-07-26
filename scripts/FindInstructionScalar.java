import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;

public class FindInstructionScalar extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException("usage: FindInstructionScalar <value>");
        }
        long expected = Long.decode(args[0]);
        for (Instruction instruction : currentProgram.getListing().getInstructions(true)) {
            boolean match = false;
            for (int operand = 0; operand < instruction.getNumOperands(); ++operand) {
                for (Object object : instruction.getOpObjects(operand)) {
                    if (object instanceof Scalar && ((Scalar)object).getUnsignedValue() == expected) {
                        match = true;
                    }
                }
            }
            if (match) {
                println(instruction.getAddress() + " " + instruction);
            }
        }
    }
}
