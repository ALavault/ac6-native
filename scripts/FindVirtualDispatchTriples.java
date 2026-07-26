// Finds adjacent lwz [vptr] / lwz [slot] / mtspr CTR triples.
// This is a read-only structural scan; it does not identify a class.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.scalar.Scalar;

public class FindVirtualDispatchTriples extends GhidraScript {
    private static String reg(Instruction instruction, int operand) {
        if (operand >= instruction.getNumOperands()) return null;
        for (Object value : instruction.getOpObjects(operand)) {
            if (value instanceof Register) return ((Register) value).getName();
        }
        return null;
    }

    private static Long scalar(Instruction instruction, int operand) {
        if (operand >= instruction.getNumOperands()) return null;
        for (Object value : instruction.getOpObjects(operand)) {
            if (value instanceof Scalar) return ((Scalar) value).getSignedValue();
        }
        return null;
    }

    private static boolean is(Instruction instruction, String mnemonic) {
        return instruction != null && mnemonic.equals(instruction.getMnemonicString());
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        long start = args.length > 0 ? Long.decode(args[0]) : 0;
        long end = args.length > 1 ? Long.decode(args[1]) : 0xffffffffL;
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (!block.isExecute() || !block.isInitialized()) continue;
            if (block.getEnd().getOffset() < start || block.getStart().getOffset() > end) continue;
            Address from = block.getStart().getOffset() < start ? toAddr(start) : block.getStart();
            Address to = block.getEnd().getOffset() > end ? toAddr(end) : block.getEnd();
            for (Instruction first : currentProgram.getListing().getInstructions(
                    new AddressSet(from, to), true)) {
                if (!is(first, "lwz") || first.getNumOperands() < 2) continue;
                String vptr = reg(first, 0);
                String object = reg(first, 1);
                Long firstDisp = scalar(first, 1);
                if (vptr == null || object == null || firstDisp == null || firstDisp != 0L) continue;
                Instruction second = first.getNext();
                Instruction third = second == null ? null : second.getNext();
                if (!is(second, "lwz") || !is(third, "mtspr") || second.getNumOperands() < 2) continue;
                if (!vptr.equals(reg(second, 0)) || !vptr.equals(reg(second, 1))) continue;
                Long slot = scalar(second, 1);
                if (slot == null || slot != 0x20L) continue;
                if (third.getNumOperands() < 2 || !"CTR".equals(reg(third, 0))
                        || !vptr.equals(reg(third, 1))) continue;
                println("TRIPLE first=" + first.getAddress()
                    + " second=" + second.getAddress()
                    + " ctr=" + third.getAddress()
                    + " object_reg=" + object
                    + " vptr_reg=" + vptr);
            }
        }
    }
}
