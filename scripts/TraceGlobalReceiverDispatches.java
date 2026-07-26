// Traces a deliberately narrow PowerPC idiom used by the shared mode owner:
// load the owner pointer from a global, load its receiver field, then dispatch
// receiver vtable slot +0x20.  This is evidence collection only; it does not
// assign a type or a campaign meaning to a receiver.
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
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;

public class TraceGlobalReceiverDispatches extends GhidraScript {
    private static String firstRegister(Instruction instruction, int operandIndex) {
        for (Object operand : instruction.getOpObjects(operandIndex)) {
            if (operand instanceof Register) return ((Register) operand).getName();
        }
        return null;
    }

    private static Long firstScalar(Instruction instruction, int operandIndex) {
        for (Object operand : instruction.getOpObjects(operandIndex)) {
            if (operand instanceof Scalar) return ((Scalar) operand).getSignedValue();
        }
        return null;
    }

    private static boolean referencesAddress(Instruction instruction, Address address) {
        for (Reference reference : instruction.getReferencesFrom()) {
            if (address.equals(reference.getToAddress())) return true;
        }
        return false;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException(
                "usage: TraceGlobalReceiverDispatches <global-address> <receiver-field>");
        }
        Address globalAddress = toAddr(Long.decode(args[0]));
        long receiverField = Long.decode(args[1]);
        Set<Address> functionEntries = new HashSet<>();
        for (Reference reference : currentProgram.getReferenceManager().getReferencesTo(globalAddress)) {
            Function function = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            if (function != null) functionEntries.add(function.getEntryPoint());
        }

        for (Address entry : functionEntries) {
            Function function = currentProgram.getFunctionManager().getFunctionAt(entry);
            if (function == null) continue;
            Map<String, Address> ownerRegisters = new HashMap<>();
            Map<String, Address> receiverRegisters = new HashMap<>();
            Map<String, Address> vtableRegisters = new HashMap<>();
            Map<String, Address> slotRegisters = new HashMap<>();
            String lastArgumentFour = "unknown";
            for (Instruction instruction : currentProgram.getListing()
                    .getInstructions(function.getBody(), true)) {
                String mnemonic = instruction.getMnemonicString();
                String destination = instruction.getNumOperands() > 0
                    ? firstRegister(instruction, 0) : null;
                String base = instruction.getNumOperands() > 1
                    ? firstRegister(instruction, 1) : null;
                Long displacement = instruction.getNumOperands() > 1
                    ? firstScalar(instruction, 1) : null;

                if (mnemonic.equals("li") && "r4".equals(destination) && instruction.getNumOperands() > 1) {
                    Long value = firstScalar(instruction, 1);
                    if (value != null) lastArgumentFour = "0x" + Long.toHexString(value);
                }

                if (mnemonic.equals("lwz") && referencesAddress(instruction, globalAddress)
                        && destination != null) {
                    ownerRegisters.put(destination, instruction.getAddress());
                    continue;
                }
                if (mnemonic.equals("or") && instruction.getNumOperands() >= 3) {
                    String left = firstRegister(instruction, 1);
                    String right = firstRegister(instruction, 2);
                    if (destination != null && left != null && left.equals(right)) {
                        if (ownerRegisters.containsKey(left)) ownerRegisters.put(destination, ownerRegisters.get(left));
                        if (receiverRegisters.containsKey(left)) receiverRegisters.put(destination, receiverRegisters.get(left));
                        if (vtableRegisters.containsKey(left)) vtableRegisters.put(destination, vtableRegisters.get(left));
                        if (slotRegisters.containsKey(left)) slotRegisters.put(destination, slotRegisters.get(left));
                        continue;
                    }
                }
                if (mnemonic.equals("lwz") && destination != null && base != null && displacement != null) {
                    if (ownerRegisters.containsKey(base) && displacement == receiverField) {
                        receiverRegisters.put(destination, instruction.getAddress());
                        continue;
                    }
                    if (receiverRegisters.containsKey(base) && displacement == 0L) {
                        vtableRegisters.put(destination, instruction.getAddress());
                        continue;
                    }
                    if (vtableRegisters.containsKey(base) && displacement == 0x20L) {
                        slotRegisters.put(destination, instruction.getAddress());
                        continue;
                    }
                }
                if (mnemonic.equals("mtspr") && instruction.getNumOperands() > 1
                        && "CTR".equals(firstRegister(instruction, 0))) {
                    String source = firstRegister(instruction, 1);
                    if (source != null && slotRegisters.containsKey(source)) {
                        println("DISPATCH function=" + function.getEntryPoint()
                            + " owner_load=" + ownerRegisters.values()
                            + " receiver_load=" + receiverRegisters.values()
                            + " vtable_load=" + vtableRegisters.values()
                            + " slot_load=" + slotRegisters.get(source)
                            + " ctr=" + instruction.getAddress()
                            + " last_r4=" + lastArgumentFour);
                    }
                }
                if (destination != null) {
                    ownerRegisters.remove(destination);
                    receiverRegisters.remove(destination);
                    vtableRegisters.remove(destination);
                    slotRegisters.remove(destination);
                }
            }
        }
    }
}
