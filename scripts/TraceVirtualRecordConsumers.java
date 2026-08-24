// Report slot +0xb0 dispatches and the bounded instruction window after bctrl.
// Candidate finder only: it does not infer object type or record semantics.
// Usage: TraceVirtualRecordConsumers.java START END
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.lang.Register;
import ghidra.program.model.scalar.Scalar;

public class TraceVirtualRecordConsumers extends GhidraScript {
    private static String reg(Instruction instruction, int operand) {
        if (operand >= instruction.getNumOperands()) return null;
        for (Object value : instruction.getOpObjects(operand)) {
            if (value instanceof Register) return ((Register)value).getName();
        }
        return null;
    }

    private static Long scalar(Instruction instruction, int operand) {
        if (operand >= instruction.getNumOperands()) return null;
        for (Object value : instruction.getOpObjects(operand)) {
            if (value instanceof Scalar) return ((Scalar)value).getSignedValue();
        }
        return null;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) throw new IllegalArgumentException(
            "usage: TraceVirtualRecordConsumers.java START END");
        long start = Long.decode(args[0]);
        long end = Long.decode(args[1]);
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (!block.isExecute() || !block.isInitialized()) continue;
            if (block.getEnd().getOffset() < start || block.getStart().getOffset() > end) continue;
            Address from = block.getStart().getOffset() < start ? toAddr(start) : block.getStart();
            Address to = block.getEnd().getOffset() > end ? toAddr(end) : block.getEnd();
            String vtable = null;
            String slot = null;
            Address slotLoad = null;
            for (Instruction instruction : currentProgram.getListing()
                    .getInstructions(new AddressSet(from, to), true)) {
                String mnemonic = instruction.getMnemonicString();
                String dst = reg(instruction, 0);
                String base = reg(instruction, 1);
                Long displacement = scalar(instruction, 1);
                if ("or".equals(mnemonic) && instruction.getNumOperands() >= 3) {
                    String left = reg(instruction, 1);
                    String right = reg(instruction, 2);
                    if (dst != null && left != null && left.equals(right)) {
                        if (left.equals(vtable)) vtable = dst;
                        if (left.equals(slot)) slot = dst;
                    }
                    continue;
                }
                if ("lwz".equals(mnemonic) && dst != null && base != null && displacement != null) {
                    if (displacement == 0L) {
                        vtable = dst;
                        slot = null;
                    } else if (vtable != null && base.equals(vtable) && displacement == 0xb0L) {
                        slot = dst;
                        slotLoad = instruction.getAddress();
                    } else {
                        if (dst.equals(vtable)) vtable = null;
                        if (dst.equals(slot)) slot = null;
                    }
                } else if ("mtspr".equals(mnemonic) && slot != null
                        && "CTR".equals(reg(instruction, 0))
                        && slot.equals(reg(instruction, 1))) {
                    Instruction call = instruction.getNext();
                    if (call == null || !"bctrl".equals(call.getMnemonicString())) continue;
                    Function owner = currentProgram.getFunctionManager()
                        .getFunctionContaining(instruction.getAddress());
                    println("CANDIDATE dispatch=" + instruction.getAddress()
                        + " call=" + call.getAddress()
                        + " slot_load=" + slotLoad
                        + " function=" + (owner == null ? "<no-function>" :
                            owner.getEntryPoint() + ":" + owner.getName()));
                    Instruction after = call.getNext();
                    for (int i = 0; i < 12 && after != null; ++i, after = after.getNext())
                        println("  AFTER " + after.getAddress() + " " + after);
                } else if (dst != null) {
                    if (dst.equals(vtable)) vtable = null;
                    if (dst.equals(slot)) slot = null;
                }
            }
        }
    }
}
