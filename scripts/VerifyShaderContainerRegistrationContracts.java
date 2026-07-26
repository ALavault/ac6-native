// Read-only assertions for the AC6 PAL active shader-container registration path.
// The contract stops at the shader-description entry boundary; it does not
// assign MATE technique/pass/permutation semantics.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Instruction;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class VerifyShaderContainerRegistrationContracts extends GhidraScript {
    private int assertions;

    private void requireInstruction(long value, String mnemonic, String fragment)
            throws Exception {
        Address address = toAddr(value);
        Instruction instruction = currentProgram.getListing().getInstructionAt(address);
        if (instruction == null) {
            Disassembler.getDisassembler(currentProgram, monitor, null)
                .disassemble(address, null);
            instruction = currentProgram.getListing().getInstructionAt(address);
        }
        if (instruction == null || !mnemonic.equals(instruction.getMnemonicString()) ||
                !instruction.toString().contains(fragment)) {
            throw new IllegalStateException(address + " expected " + mnemonic +
                " containing " + fragment + " but found " +
                (instruction == null ? "<none>" : instruction));
        }
        assertions++;
        println("AC6_SHADER_CONTAINER_CONTRACT=" + address + " " + instruction);
    }

    private void requireWord(long value, long expected) throws Exception {
        Address address = toAddr(value);
        long actual = Integer.toUnsignedLong(currentProgram.getMemory().getInt(address));
        if (actual != expected) {
            throw new IllegalStateException(address + " expected 0x" +
                Long.toHexString(expected) + " but found 0x" + Long.toHexString(actual));
        }
        assertions++;
        println("AC6_SHADER_CONTAINER_WORD=" + address + " 0x" +
            Long.toHexString(actual));
    }

    private void requireEntry163RegistrationPermutation() throws Exception {
        AddressSet body = new AddressSet(toAddr(0x821d5660L), toAddr(0x821d5ec8L));
        List<Integer> indices = new ArrayList<>();
        Integer currentR4 = null;
        Integer accessorIndex = null;

        for (Instruction instruction : currentProgram.getListing().getInstructions(body, true)) {
            String text = instruction.toString();
            if (text.startsWith("li r4,0x")) {
                currentR4 = Integer.parseUnsignedInt(text.substring("li r4,0x".length()), 16);
            } else if (text.equals("bl 0x82234dd0")) {
                accessorIndex = currentR4;
            } else if (text.equals("bl 0x82338500")) {
                if (accessorIndex == null) {
                    throw new IllegalStateException(instruction.getAddress() +
                        " registration has no preceding packed-subrecord index");
                }
                indices.add(accessorIndex);
            }
        }

        if (indices.size() != 50) {
            throw new IllegalStateException("expected 50 shader-container registrations but found " +
                indices.size());
        }
        assertions++;

        List<Integer> sorted = new ArrayList<>(indices);
        Collections.sort(sorted);
        for (int expected = 0; expected < 50; expected++) {
            if (sorted.get(expected) != expected) {
                throw new IllegalStateException("entry-163 registration indices are not a " +
                    "permutation of 0..49: " + indices);
            }
            assertions++;
        }
        println("AC6_SHADER_CONTAINER_ENTRY163_REGISTRATIONS=" + indices);
    }

    @Override
    public void run() throws Exception {
        // The only accepted four-byte container signature is reached through
        // the one-entry pointer table at 0x826762a0 and spells "NSXR".
        requireWord(0x826762a0L, 0x820110ccL);
        requireWord(0x820110ccL, 0x4e535852L);

        // Active container iterator: validate the signature, obtain the
        // big-endian entry count and first payload entry, then derive a key.
        requireInstruction(0x82338530L, "bl", "0x82344148");
        requireInstruction(0x82338548L, "bl", "0x823440a0");
        requireInstruction(0x82338554L, "bl", "0x823440a8");
        requireInstruction(0x8233858cL, "bl", "0x823369e0");

        // The call preserves category, current description entry and flag as
        // r4/r6/r7, while the service and derived key occupy r3/r5.
        requireInstruction(0x82338570L, "subi", "r28,r11,0x3d80");
        requireInstruction(0x82338590L, "or.", "r5,r3,r3");
        requireInstruction(0x82338598L, "or", "r7,r27,r27");
        requireInstruction(0x8233859cL, "or", "r6,r31,r31");
        requireInstruction(0x823385a0L, "or", "r4,r26,r26");
        requireInstruction(0x823385a4L, "or", "r3,r28,r28");
        requireInstruction(0x823385a8L, "bl", "0x82343f60");
        requireInstruction(0x823385b4L, "bl", "0x823440b0");

        // Representative resource-table consumer: index 0 resolves through
        // the qualified packed-subrecord accessor and registers category 0x40.
        requireInstruction(0x821d5660L, "li", "r4,0x0");
        requireInstruction(0x821d5664L, "addi", "r3,r31,0x8");
        requireInstruction(0x821d5668L, "bl", "0x82234dd0");
        requireInstruction(0x821d566cL, "li", "r4,0x0");
        requireInstruction(0x821d5670L, "li", "r5,0x0");
        requireInstruction(0x821d5674L, "li", "r6,0x40");
        requireInstruction(0x821d5678L, "bl", "0x82338500");

        // The enclosing initializer queues the literal 0xa3 before invoking
        // the still-unqualified resource loading/waiting chain.
        requireInstruction(0x821d55c4L, "cmplwi", "r11,0xa3");
        requireInstruction(0x821d55e4L, "li", "r10,0xa3");
        requireInstruction(0x821d55ecL, "sthx", "r10,r11,r9");
        requireInstruction(0x821d5608L, "bl", "0x821cbfd0");
        requireInstruction(0x821d561cL, "bl", "0x82222e98");
        requireInstruction(0x821d5644L, "bl", "0x821cc4d0");

        // The initializer registers exactly the 50 subrecords of DATA entry
        // 0xa3, in a fixed but non-sequential permutation of indices 0..49.
        requireEntry163RegistrationPermutation();

        println("AC6_SHADER_CONTAINER_ASSERTIONS=" + assertions);
    }
}
