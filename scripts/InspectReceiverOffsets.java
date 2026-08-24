import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.lang.Register;

/** Read-only census of virtual slot-4 dispatches and nearby +12 adjustments. */
public class InspectReceiverOffsets extends GhidraScript {
    private static String reg(Instruction ins, int operand) {
        if (operand >= ins.getNumOperands()) return null;
        for (Object value : ins.getOpObjects(operand))
            if (value instanceof Register) return ((Register) value).getName();
        return null;
    }
    private static Long scalar(Instruction ins, int operand) {
        if (operand >= ins.getNumOperands()) return null;
        for (Object value : ins.getOpObjects(operand))
            if (value instanceof Scalar) return ((Scalar) value).getSignedValue();
        return null;
    }
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        long start = args.length > 0 ? Long.decode(args[0]) : 0;
        long end = args.length > 1 ? Long.decode(args[1]) : 0xffffffffL;
        int back = args.length > 2 ? Integer.decode(args[2]) : 32;
        for (Instruction ins : currentProgram.getListing().getInstructions(true)) {
            long ea = ins.getAddress().getOffset();
            if (ea < start || ea > end || !"mtspr".equals(ins.getMnemonicString())) continue;
            if (!"CTR".equals(reg(ins, 0))) continue;
            Instruction slotLoad = ins.getPrevious();
            Instruction vtableLoad = slotLoad == null ? null : slotLoad.getPrevious();
            if (slotLoad == null || vtableLoad == null
                    || !"lwz".equals(slotLoad.getMnemonicString())
                    || scalar(slotLoad, 1) == null || scalar(slotLoad, 1) != 4L
                    || !"lwz".equals(vtableLoad.getMnemonicString())
                    || scalar(vtableLoad, 1) == null || scalar(vtableLoad, 1) != 0L)
                continue;
            String vreg = reg(vtableLoad, 0);
            String receiver = reg(vtableLoad, 1);
            if (vreg == null || receiver == null || !vreg.equals(reg(slotLoad, 0))) continue;
            Function fn = currentProgram.getFunctionManager().getFunctionContaining(ins.getAddress());
            println("DISPATCH " + ins.getAddress() + " function="
                    + (fn == null ? "<no-function>" : fn.getEntryPoint() + " " + fn.getName())
                    + " receiver=" + receiver + " slot_reg=" + vreg);
            Instruction cursor = vtableLoad;
            for (int i = 0; i < back && cursor != null; ++i, cursor = cursor.getPrevious()) {
                String mnemonic = cursor.getMnemonicString();
                String dst = reg(cursor, 0);
                Long imm = scalar(cursor, 2);
                if ("addi".equals(mnemonic) && receiver.equals(dst) && imm != null && imm == 12L)
                    println("  ADJUST +12 " + cursor.getAddress() + " " + cursor);
                if ("lwz".equals(mnemonic) && receiver.equals(dst)
                        && scalar(cursor, 1) != null && scalar(cursor, 1) == 12L)
                    println("  LOAD +12 " + cursor.getAddress() + " " + cursor);
            }
        }
    }
}
