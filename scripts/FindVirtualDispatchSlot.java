// Finds adjacent Xenon PPC virtual dispatches for one vtable slot.
// Read-only structural scan; every hit still requires decompiler qualification.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.scalar.Scalar;

public class FindVirtualDispatchSlot extends GhidraScript {
    private static String reg(Instruction i, int operand) {
        if (operand >= i.getNumOperands()) return null;
        for (Object value : i.getOpObjects(operand)) {
            if (value instanceof Register) return ((Register)value).getName();
        }
        return null;
    }

    private static Long scalar(Instruction i, int operand) {
        if (operand >= i.getNumOperands()) return null;
        for (Object value : i.getOpObjects(operand)) {
            if (value instanceof Scalar) return ((Scalar)value).getSignedValue();
        }
        return null;
    }

    private static boolean is(Instruction i, String mnemonic) {
        return i != null && mnemonic.equals(i.getMnemonicString());
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException(
            "usage: FindVirtualDispatchSlot <slot-byte-offset>");
        long slot = Long.decode(args[0]);
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (!block.isExecute() || !block.isInitialized()) continue;
            for (Instruction first : currentProgram.getListing().getInstructions(
                    new AddressSet(block.getStart(), block.getEnd()), true)) {
                if (!is(first, "lwz")) continue;
                String fn = reg(first, 0);
                String obj = reg(first, 1);
                Long vptrOffset = scalar(first, 1);
                if (fn == null || obj == null || vptrOffset == null || vptrOffset != 0) continue;
                Instruction second = first.getNext();
                Instruction third = second == null ? null : second.getNext();
                Instruction fourth = third == null ? null : third.getNext();
                if (!is(second, "lwz") || !is(third, "mtspr") || !is(fourth, "bctrl")) continue;
                if (!fn.equals(reg(second, 0)) || !fn.equals(reg(second, 1))) continue;
                Long actual = scalar(second, 1);
                if (actual == null || actual != slot) continue;
                if (third.getNumOperands() < 2 || !"CTR".equals(reg(third, 0))
                        || !fn.equals(reg(third, 1))) continue;
                println("DISPATCH first=" + first.getAddress()
                    + " vptr_load=" + first
                    + " method_load=" + second
                    + " slot=" + String.format("0x%x", slot)
                    + " call=" + fourth.getAddress()
                    + " object_reg=" + obj
                    + " function_reg=" + fn);
            }
        }
    }
}
