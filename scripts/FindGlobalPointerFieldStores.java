// Finds local, register-preserving candidates for writes through a pointer
// loaded from a given global.  This is deliberately only a candidate finder:
// it is flow-insensitive across branches/calls and does not assign semantics
// to the pointed-to object or its fields.
// @category AC6

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.lang.Register;
import ghidra.program.model.symbol.Reference;

public class FindGlobalPointerFieldStores extends GhidraScript {
    private static String registerName(Object operand) {
        return operand instanceof Register ? ((Register) operand).getName() : null;
    }

    private static String firstRegister(Instruction instruction, int operandIndex) {
        Object[] operands = instruction.getOpObjects(operandIndex);
        for (Object operand : operands) {
            String name = registerName(operand);
            if (name != null) {
                return name;
            }
        }
        return null;
    }

    private static boolean referencesAddress(Instruction instruction, Address address) {
        for (Reference reference : instruction.getReferencesFrom()) {
            if (address.equals(reference.getToAddress())) {
                return true;
            }
        }
        return false;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException(
                "usage: FindGlobalPointerFieldStores <global-address> <field-displacement>");
        }
        Address globalAddress = toAddr(Long.decode(args[0]));
        long fieldDisplacement = Long.decode(args[1]);
        Set<Address> functionEntries = new HashSet<>();
        for (Reference reference : currentProgram.getReferenceManager().getReferencesTo(globalAddress)) {
            Function function = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            if (function != null) {
                functionEntries.add(function.getEntryPoint());
            }
        }

        for (Address entry : functionEntries) {
            Function function = currentProgram.getFunctionManager().getFunctionAt(entry);
            if (function == null) {
                continue;
            }
            Map<String, Address> aliases = new HashMap<>();
            for (Instruction instruction : currentProgram.getListing()
                    .getInstructions(function.getBody(), true)) {
                String mnemonic = instruction.getMnemonicString();
                String text = instruction.toString();
                String destination = firstRegister(instruction, 0);

                if (referencesAddress(instruction, globalAddress) && mnemonic.equals("lwz")) {
                    if (destination != null) {
                        aliases.put(destination, instruction.getAddress());
                    }
                    continue;
                }

                if (mnemonic.equals("or") && instruction.getNumOperands() >= 3) {
                    String sourceOne = firstRegister(instruction, 1);
                    String sourceTwo = firstRegister(instruction, 2);
                    if (destination != null && sourceOne != null && sourceOne.equals(sourceTwo)
                            && aliases.containsKey(sourceOne)) {
                        aliases.put(destination, aliases.get(sourceOne));
                        continue;
                    }
                }

                if (mnemonic.equals("stw") && instruction.getNumOperands() >= 2) {
                    String baseRegister = firstRegister(instruction, 1);
                    if (baseRegister != null && aliases.containsKey(baseRegister)
                            && text.contains("0x" + Long.toHexString(fieldDisplacement) + "(")) {
                        println("CANDIDATE function=" + function.getEntryPoint()
                            + " global_load=" + aliases.get(baseRegister)
                            + " store=" + instruction.getAddress() + " " + instruction);
                    }
                }

                // Most PowerPC instructions write operand zero.  Once a tracked
                // register is overwritten by anything other than the copy above,
                // stop treating it as the global pointer.
                if (destination != null && aliases.containsKey(destination)) {
                    aliases.remove(destination);
                }
            }
        }
    }
}
