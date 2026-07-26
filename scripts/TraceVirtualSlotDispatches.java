// Finds direct PPC virtual-slot dispatch idioms in executable memory.
// Read-only candidate finder; it does not infer a class or event meaning.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.lang.Register;
import ghidra.program.model.scalar.Scalar;

public class TraceVirtualSlotDispatches extends GhidraScript {
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

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        long start = args.length > 0 ? Long.decode(args[0]) : 0;
        long end = args.length > 1 ? Long.decode(args[1]) : 0xffffffffL;
        long expectedSlot = args.length > 2 ? Long.decode(args[2]) : 0x20L;

        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (!block.isExecute() || !block.isInitialized()) continue;
            Address blockStart = block.getStart();
            Address blockEnd = block.getEnd();
            if (blockEnd.getOffset() < start || blockStart.getOffset() > end) continue;
            Address from = blockStart.getOffset() < start ? toAddr(start) : blockStart;
            Address to = blockEnd.getOffset() > end ? toAddr(end) : blockEnd;
            String[] vtableRegister = { null };
            String[] slotRegister = { null };
            Address[] slotLoad = { null };
            for (Instruction instruction : currentProgram.getListing().getInstructions(
                    new AddressSet(from, to), true)) {
                String mnemonic = instruction.getMnemonicString();
                String dst = reg(instruction, 0);
                String base = reg(instruction, 1);
                Long displacement = scalar(instruction, 1);

                if ("or".equals(mnemonic) && instruction.getNumOperands() >= 3) {
                    String left = reg(instruction, 1);
                    String right = reg(instruction, 2);
                    if (dst != null && left != null && left.equals(right)) {
                        if (left.equals(vtableRegister[0])) vtableRegister[0] = dst;
                        if (left.equals(slotRegister[0])) slotRegister[0] = dst;
                        continue;
                    }
                }

                if ("lwz".equals(mnemonic) && dst != null && base != null && displacement != null) {
                    if (displacement == 0L) {
                        vtableRegister[0] = dst;
                        slotRegister[0] = null;
                    } else if (vtableRegister[0] != null && base.equals(vtableRegister[0])
                            && displacement == expectedSlot) {
                        slotRegister[0] = dst;
                        slotLoad[0] = instruction.getAddress();
                    } else {
                        if (dst.equals(vtableRegister[0])) vtableRegister[0] = null;
                        if (dst.equals(slotRegister[0])) slotRegister[0] = null;
                    }
                } else if ("mtspr".equals(mnemonic) && instruction.getNumOperands() > 1
                        && "CTR".equals(reg(instruction, 0))
                        && slotRegister[0] != null && slotRegister[0].equals(reg(instruction, 1))) {
                    Function owner = currentProgram.getFunctionManager()
                        .getFunctionContaining(instruction.getAddress());
                    println("DISPATCH address=" + instruction.getAddress()
                        + " function=" + (owner == null ? "<no-function>" :
                            owner.getEntryPoint() + ":" + owner.getName())
                        + " slot_load=" + slotLoad[0]
                        + " vtable_reg=" + vtableRegister[0]
                        + " slot_reg=" + slotRegister[0]);
                } else if (dst != null) {
                    if (dst.equals(vtableRegister[0])) vtableRegister[0] = null;
                    if (dst.equals(slotRegister[0])) slotRegister[0] = null;
                }
            }
        }
    }
}
