// Trace virtual calls made from functions that load an object from a global.
// This is a narrow, flow-sensitive candidate finder for one virtual slot.
// Usage: TraceGlobalVirtualSlot <global-address> <receiver-field> <slot-displacement>
// It does not identify the class or assign semantics to the slot.
// @category AC6

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;

public class TraceGlobalVirtualSlot extends GhidraScript {
    private static String register(Instruction instruction, int index) {
        if (index >= instruction.getNumOperands()) return null;
        for (Object operand : instruction.getOpObjects(index)) {
            if (operand instanceof Register) return ((Register) operand).getName();
        }
        return null;
    }

    private static Long scalar(Instruction instruction, int index) {
        if (index >= instruction.getNumOperands()) return null;
        for (Object operand : instruction.getOpObjects(index)) {
            if (operand instanceof Scalar) return ((Scalar) operand).getSignedValue();
        }
        return null;
    }

    private static boolean references(Instruction instruction, Address address) {
        for (Reference reference : instruction.getReferencesFrom()) {
            if (address.equals(reference.getToAddress())) return true;
        }
        return false;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 3) {
            throw new IllegalArgumentException(
                "usage: TraceGlobalVirtualSlot <global-address> <receiver-field> <slot-displacement>");
        }
        Address global = toAddr(Long.decode(args[0]));
        long receiverField = Long.decode(args[1]);
        long slot = Long.decode(args[2]);
        Set<Address> entries = new HashSet<>();
        for (Reference reference : currentProgram.getReferenceManager().getReferencesTo(global)) {
            Function function = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            if (function != null) entries.add(function.getEntryPoint());
        }

        for (Address entry : entries) {
            Function function = currentProgram.getFunctionManager().getFunctionAt(entry);
            if (function == null) continue;
            Map<String, String> owners = new HashMap<>();
            Map<String, String> receivers = new HashMap<>();
            Map<String, String> vtables = new HashMap<>();
            Map<String, String> slots = new HashMap<>();
            for (Instruction instruction : currentProgram.getListing()
                    .getInstructions(function.getBody(), true)) {
                String mnemonic = instruction.getMnemonicString();
                String destination = register(instruction, 0);
                String base = register(instruction, 1);
                Long displacement = scalar(instruction, 1);

                if (mnemonic.equals("lwz") && destination != null
                        && references(instruction, global)) {
                    owners.put(destination, instruction.getAddress().toString());
                    continue;
                }
                if (mnemonic.equals("or") && destination != null
                        && instruction.getNumOperands() >= 3) {
                    String left = register(instruction, 1);
                    String right = register(instruction, 2);
                    if (left != null && left.equals(right)) {
                        if (owners.containsKey(left)) owners.put(destination, owners.get(left));
                        if (receivers.containsKey(left)) receivers.put(destination, receivers.get(left));
                        if (vtables.containsKey(left)) vtables.put(destination, vtables.get(left));
                        if (slots.containsKey(left)) slots.put(destination, slots.get(left));
                        continue;
                    }
                }
                if (mnemonic.equals("lwz") && destination != null && base != null
                        && displacement != null) {
                    if (owners.containsKey(base) && displacement == receiverField) {
                        receivers.put(destination, instruction.getAddress().toString());
                        continue;
                    }
                    if (receivers.containsKey(base) && displacement == 0L) {
                        vtables.put(destination, instruction.getAddress().toString());
                        continue;
                    }
                    if (vtables.containsKey(base) && displacement == slot) {
                        slots.put(destination, instruction.getAddress().toString());
                        continue;
                    }
                }
                if (mnemonic.equals("mtspr") && instruction.getNumOperands() > 1
                        && "CTR".equals(register(instruction, 0))) {
                    String source = register(instruction, 1);
                    if (source != null && slots.containsKey(source)) {
                        println("DISPATCH function=" + function.getEntryPoint()
                            + " owner_load=" + owners.values()
                            + " receiver_load=" + receivers.values()
                            + " vtable_load=" + vtables.values()
                            + " slot_load=" + slots.get(source)
                            + " ctr=" + instruction.getAddress());
                    }
                }
                if (destination != null) {
                    owners.remove(destination);
                    receivers.remove(destination);
                    vtables.remove(destination);
                    slots.remove(destination);
                }
            }
        }
    }
}
