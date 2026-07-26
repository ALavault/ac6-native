// Trace a narrow, read-only owner-pointer flow in all defined executable
// instructions, including instruction islands without a Ghidra Function.
// This is a candidate finder only: it does not assign a type or campaign
// meaning to the owner or to any callee.
// @category AC6

import java.util.HashMap;
import java.util.Map;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;

public class TraceGlobalOwnerFlow extends GhidraScript {
    private static String firstRegister(Instruction instruction, int operandIndex) {
        for (Object operand : instruction.getOpObjects(operandIndex)) {
            if (operand instanceof Register) {
                return ((Register) operand).getName();
            }
        }
        return null;
    }

    private static Long firstScalar(Instruction instruction, int operandIndex) {
        for (Object operand : instruction.getOpObjects(operandIndex)) {
            if (operand instanceof Scalar) {
                return ((Scalar) operand).getSignedValue();
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

    private static boolean isDirectCall(Instruction instruction) {
        return "bl".equals(instruction.getMnemonicString())
            && instruction.getFlows().length > 0;
    }

    private void traceBlock(MemoryBlock block, Address global) throws Exception {
        Map<String, Address> ownerAliases = new HashMap<>();
        Address previous = null;
        AddressSet range = new AddressSet(block.getStart(), block.getEnd());
        for (Instruction instruction : currentProgram.getListing().getInstructions(range, true)) {
            if (previous == null || !previous.add(4).equals(instruction.getAddress())) {
                ownerAliases.clear();
            }
            previous = instruction.getAddress();

            String mnemonic = instruction.getMnemonicString();
            String destination = instruction.getNumOperands() > 0
                ? firstRegister(instruction, 0) : null;

            if ("lwz".equals(mnemonic) && referencesAddress(instruction, global)
                    && destination != null) {
                ownerAliases.put(destination, instruction.getAddress());
                println("OWNER_LOAD address=" + instruction.getAddress()
                    + " register=" + destination);
                continue;
            }

            if ("or".equals(mnemonic) && instruction.getNumOperands() >= 3) {
                String left = firstRegister(instruction, 1);
                String right = firstRegister(instruction, 2);
                if (destination != null && left != null && left.equals(right)
                        && ownerAliases.containsKey(left)) {
                    ownerAliases.put(destination, ownerAliases.get(left));
                    continue;
                }
            }

            if ("stw".equals(mnemonic) && instruction.getNumOperands() >= 2) {
                String base = firstRegister(instruction, 1);
                Long displacement = firstScalar(instruction, 1);
                if (base != null && displacement != null && displacement == 8L
                        && ownerAliases.containsKey(base)) {
                    println("DIRECT_STORE owner_load=" + ownerAliases.get(base)
                        + " store=" + instruction.getAddress() + " " + instruction);
                }
            }

            if (isDirectCall(instruction)
                    && (ownerAliases.containsKey("r3") || ownerAliases.containsKey("r4")
                        || ownerAliases.containsKey("r5") || ownerAliases.containsKey("r6"))) {
                StringBuilder arguments = new StringBuilder();
                for (String register : new String[] {"r3", "r4", "r5", "r6"}) {
                    if (ownerAliases.containsKey(register)) {
                        if (arguments.length() > 0) arguments.append(',');
                        arguments.append(register).append("<-")
                            .append(ownerAliases.get(register));
                    }
                }
                println("PASS_TO_CALL from=" + instruction.getAddress()
                    + " target=" + instruction.getFlows()[0]
                    + " args=" + arguments);
                ownerAliases.clear();
                continue;
            }

            // A non-copy write destroys an alias.  This remains intentionally
            // local and conservative; calls and branches clear all aliases.
            if (destination != null && ownerAliases.containsKey(destination)) {
                ownerAliases.remove(destination);
            }
            if ("bl".equals(mnemonic) || "b".equals(mnemonic)
                    || "bctr".equals(mnemonic) || "blr".equals(mnemonic)) {
                ownerAliases.clear();
            }
        }
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException("usage: TraceGlobalOwnerFlow <global-address>");
        }
        Address global = toAddr(Long.decode(args[0]));
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (block.isExecute() && block.isInitialized()) {
                traceBlock(block, global);
            }
        }
    }
}
