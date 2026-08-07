// Recover the references PPC hides in split address materializations.
//
// PowerPC builds a 32-bit address in two instructions, `lis rD,HI` then
// `addi rD,rA,LO` or `ori rD,rS,LO`. Ghidra creates no reference for the pair,
// so 3921 of 4968 strings in this program report zero references and every
// owner search over them returns a false negative. Cycle 1036 rejected
// H-GHIDRA-SUBMISTBL-STRING-REFERENCE on exactly that basis.
//
// The propagation must be register-aware. One `lis` commonly feeds several
// low halves: at 0x823096C0 the compiler emits `lis r11,0x8201`, then an
// unrelated `addi r29,r27,0x4`, then `addi r24,r11,-0xa58` -> 0x8200F5A8.
// Pairing a lis with the next addi regardless of register reports the wrong
// constant and hides the real one.
//
// Scope. Only pairs resolving into mapped memory produce a reference, and the
// high half is invalidated as soon as any other instruction writes its
// register. This is a reference-recovery pass, not a constant propagator: it
// makes no claim about values that reach memory or cross a call.
//
// Run WITHOUT -readOnly. Idempotent: an existing reference to the same target
// is not duplicated.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.SourceType;

public class RecoverPpcSplitAddressReferences extends GhidraScript {

    private static final String QUALIFIED_XEX_SHA256 =
        "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";

    private static long word(Instruction instruction) throws Exception {
        byte[] bytes = instruction.getBytes();
        return ((long) (bytes[0] & 0xff) << 24) | ((long) (bytes[1] & 0xff) << 16)
             | ((long) (bytes[2] & 0xff) << 8)  | (long) (bytes[3] & 0xff);
    }

    private static int signed16(long value) {
        int result = (int) (value & 0xffffL);
        return result >= 0x8000 ? result - 0x10000 : result;
    }

    private boolean alreadyReferenced(Instruction instruction, Address target) {
        for (Reference reference : instruction.getReferencesFrom()) {
            if (target.equals(reference.getToAddress())) {
                return true;
            }
        }
        return false;
    }

    /** Invalidates any tracked high half whose register this instruction writes. */
    private void invalidateResults(Instruction instruction, Long[] high, int skip) {
        for (Object result : instruction.getResultObjects()) {
            if (!(result instanceof Register)) {
                continue;
            }
            String name = ((Register) result).getName();
            if (!name.startsWith("r")) {
                continue;
            }
            try {
                int index = Integer.parseInt(name.substring(1));
                if (index >= 0 && index < 32 && index != skip) {
                    high[index] = null;
                }
            } catch (NumberFormatException ignored) {
                // Not a numbered GPR; nothing tracked for it.
            }
        }
    }

    @Override
    protected void run() throws Exception {
        String sha = currentProgram.getExecutableSHA256();
        if (!QUALIFIED_XEX_SHA256.equalsIgnoreCase(sha)) {
            throw new IllegalStateException("unexpected XEX SHA-256: " + sha);
        }

        long pairs = 0;
        long created = 0;
        long alreadyPresent = 0;
        long unmapped = 0;
        int functions = 0;

        FunctionIterator iterator = currentProgram.getFunctionManager().getFunctions(true);
        while (iterator.hasNext()) {
            if (monitor.isCancelled()) {
                break;
            }
            Function function = iterator.next();
            functions++;
            Long[] high = new Long[32];

            InstructionIterator instructions =
                currentProgram.getListing().getInstructions(function.getBody(), true);
            while (instructions.hasNext()) {
                Instruction instruction = instructions.next();
                if (instruction.getLength() != 4) {
                    continue;
                }
                long encoding = word(instruction);
                int opcode = (int) (encoding >>> 26);

                // addis rD,rA,SI. With rA == r0 this is lis: a fresh high half.
                if (opcode == 15) {
                    int destination = (int) ((encoding >>> 21) & 31);
                    int source = (int) ((encoding >>> 16) & 31);
                    long shifted = (((long) signed16(encoding)) << 16) & 0xffffffffL;
                    if (source == 0) {
                        high[destination] = shifted;
                    } else if (high[source] != null) {
                        high[destination] = (high[source] + shifted) & 0xffffffffL;
                    } else {
                        high[destination] = null;
                    }
                    continue;
                }

                boolean addi = opcode == 14;
                boolean ori = opcode == 24;
                if (!addi && !ori) {
                    invalidateResults(instruction, high, -1);
                    continue;
                }

                int source = (int) (addi ? (encoding >>> 16) & 31 : (encoding >>> 21) & 31);
                int destination = (int) (addi ? (encoding >>> 21) & 31 : (encoding >>> 16) & 31);
                // addi rD,r0,SI is li: a constant, not a completed pointer.
                Long base = (addi && source == 0) ? null : high[source];
                if (base == null) {
                    high[destination] = null;
                    continue;
                }
                long value = addi
                    ? (base + signed16(encoding)) & 0xffffffffL
                    : (base | (encoding & 0xffffL)) & 0xffffffffL;
                // The pointer is complete; the register no longer holds a high half.
                high[destination] = null;
                pairs++;

                Address target;
                try {
                    target = toAddr(value);
                } catch (Exception malformed) {
                    unmapped++;
                    continue;
                }
                if (!currentProgram.getMemory().contains(target)) {
                    unmapped++;
                    continue;
                }
                if (alreadyReferenced(instruction, target)) {
                    alreadyPresent++;
                    continue;
                }
                int operand = Math.max(0, instruction.getNumOperands() - 1);
                currentProgram.getReferenceManager().addMemoryReference(
                    instruction.getAddress(), target, RefType.DATA,
                    SourceType.ANALYSIS, operand);
                created++;
            }
        }

        println(String.format(
            "AC6_SPLIT_REFS functions=%d pairs=%d created=%d already_present=%d unmapped=%d",
            functions, pairs, created, alreadyPresent, unmapped));

        Address submistbl = toAddr(0x8200f5a8L);
        println("AC6_SUBMISTBL_XREF_COUNT "
            + getReferencesTo(submistbl).length + " expected_site=0x823096c8");
    }
}
