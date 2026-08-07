// Find every register-relative address materialization whose effective offset
// matches one of the requested values.
//
// PowerPC forms an address inside a large structure with a pair:
//
//     addis rD,rS,HIGH        ; rS is the structure base, kept in a register
//     addi  rD,rD,LOW         ; LOW is signed, so Ghidra renders it as subi
//
// The effective offset is (HIGH << 16) + signed(LOW). This script walks the
// listing, remembers the pending high half per destination register together
// with the base register it came from, and reports the sites whose completed
// offset is one the caller asked about. It never modifies the program.
//
// usage: FindRegisterRelativeOffset.java OFFSET[,OFFSET...]
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.scalar.Scalar;

import java.util.HashSet;
import java.util.Set;

public class FindRegisterRelativeOffset extends GhidraScript {

    private static final int REGISTERS = 32;

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            printerr("usage: FindRegisterRelativeOffset.java OFFSET[,OFFSET...]");
            return;
        }
        Set<Long> wanted = new HashSet<>();
        for (String piece : args[0].split(",")) {
            wanted.add(Long.decode(piece.trim()) & 0xFFFFFFFFL);
        }

        // pendingHigh[d] is the high half addis put in register d, or null.
        // pendingBase[d] is the register that high half was formed from.
        Long[] pendingHigh = new Long[REGISTERS];
        Integer[] pendingBase = new Integer[REGISTERS];
        Address[] pendingSite = new Address[REGISTERS];

        int hits = 0;
        InstructionIterator instructions =
                currentProgram.getListing().getInstructions(true);
        while (instructions.hasNext() && !monitor.isCancelled()) {
            Instruction instruction = instructions.next();
            String mnemonic = instruction.getMnemonicString();

            if (mnemonic.equals("addis")) {
                Integer d = registerIndex(instruction, 0);
                Integer s = registerIndex(instruction, 1);
                Scalar high = scalar(instruction, 2);
                if (d != null && s != null && high != null && s != 0) {
                    pendingHigh[d] = high.getUnsignedValue() << 16;
                    pendingBase[d] = s;
                    pendingSite[d] = instruction.getAddress();
                    continue;
                }
                if (d != null) {
                    pendingHigh[d] = null;
                }
                continue;
            }

            if (mnemonic.equals("addi") || mnemonic.equals("subi")) {
                Integer d = registerIndex(instruction, 0);
                Integer s = registerIndex(instruction, 1);
                Scalar low = scalar(instruction, 2);
                if (d != null && s != null && low != null && d.equals(s)
                        && pendingHigh[d] != null) {
                    long value = low.getUnsignedValue();
                    long signed = mnemonic.equals("subi") ? -value : (short) value;
                    long effective = (pendingHigh[d] + signed) & 0xFFFFFFFFL;
                    if (wanted.contains(effective)) {
                        Function function = currentProgram.getFunctionManager()
                                .getFunctionContaining(instruction.getAddress());
                        println(String.format(
                                "AC6_OFFSET_SITE offset=0x%X base=r%d high_at=%s low_at=%s function=%s",
                                effective, pendingBase[d], pendingSite[d],
                                instruction.getAddress(),
                                function == null ? "<none>"
                                        : function.getEntryPoint().toString()));
                        hits++;
                    }
                    pendingHigh[d] = null;
                    continue;
                }
                if (d != null) {
                    pendingHigh[d] = null;
                }
                continue;
            }

            // Any other write to a register invalidates its pending high half.
            for (Object result : instruction.getResultObjects()) {
                if (result instanceof Register) {
                    int index = ((Register) result).getOffset() / 8;
                    if (index >= 0 && index < REGISTERS) {
                        pendingHigh[index] = null;
                    }
                }
            }
        }
        println("AC6_OFFSET_SITES total=" + hits);
    }

    private Integer registerIndex(Instruction instruction, int operand) {
        Object[] objects = instruction.getOpObjects(operand);
        if (objects.length != 1 || !(objects[0] instanceof Register)) {
            return null;
        }
        Register register = (Register) objects[0];
        String name = register.getName();
        if (!name.startsWith("r")) {
            return null;
        }
        try {
            return Integer.parseInt(name.substring(1));
        } catch (NumberFormatException failure) {
            return null;
        }
    }

    private Scalar scalar(Instruction instruction, int operand) {
        Object[] objects = instruction.getOpObjects(operand);
        if (objects.length != 1 || !(objects[0] instanceof Scalar)) {
            return null;
        }
        return (Scalar) objects[0];
    }
}
