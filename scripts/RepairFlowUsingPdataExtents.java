// Repair mis-flagged no-return callees and truncated bodies, using .pdata as the
// arbiter instead of a hand-maintained list of helper islands.
//
// Rule. If a call sits inside a linker-recorded function extent and is not that
// extent's last instruction, the linker states the function continues past it.
// The call therefore returns. This is authoritative evidence about the callee,
// and it needs no heuristic about what the callee looks like.
//
// This generalises the __savegprlr repair of cycle 1082. The same defect affects
// at least three more islands that a hardcoded list would have missed:
//   __savefpr_14..31 / __restfpr_14..31 at 0x82384410, based on r12 rather than
//   r1; a VMX128 island around 0x82385894; and plain `blr` stubs such as
//   0x822DDBE8 that truncate 83 functions on their own.
//
// Iterates to a fixpoint: repairing one call exposes code containing the next.
// Byte-qualified against .pdata before any change. Run WITHOUT -readOnly.
// Idempotent.
// @category AC6

import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.FlowOverride;
import java.security.MessageDigest;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.Set;

public class RepairFlowUsingPdataExtents extends GhidraScript {

    private static final String QUALIFIED_XEX_SHA256 =
        "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";

    private static final long PDATA_START = 0x82079e00L;
    private static final int  PDATA_BYTES = 65968;
    private static final String PDATA_SHA256 =
        "740f31d530dcfca2fcddab6ac6f93e1ab55d36106a9a015e41f074d5e6d73034";

    private static final long CODE_MIN = 0x82090000L;
    private static final long CODE_MAX = 0x823e7ff7L;

    private static final int MAX_ROUNDS = 8;

    private byte[] readQualifiedPdata() throws Exception {
        Address start = toAddr(PDATA_START);
        byte[] bytes = new byte[PDATA_BYTES];
        for (int index = 0; index < bytes.length; ++index) {
            bytes[index] = currentProgram.getMemory().getByte(start.add(index));
        }
        byte[] digest = MessageDigest.getInstance("SHA-256").digest(bytes);
        StringBuilder actual = new StringBuilder(64);
        for (byte value : digest) {
            actual.append(String.format("%02x", value & 0xff));
        }
        if (!PDATA_SHA256.equalsIgnoreCase(actual.toString())) {
            throw new IllegalStateException(".pdata SHA-256 mismatch: got " + actual);
        }
        println("AC6_QUALIFIED_PDATA " + start + " sha256=" + actual);
        return bytes;
    }

    private static long beU32(byte[] bytes, int offset) {
        return ((long) (bytes[offset] & 0xff) << 24)
             | ((long) (bytes[offset + 1] & 0xff) << 16)
             | ((long) (bytes[offset + 2] & 0xff) << 8)
             | (long) (bytes[offset + 3] & 0xff);
    }

    @Override
    protected void run() throws Exception {
        String sha = currentProgram.getExecutableSHA256();
        if (!QUALIFIED_XEX_SHA256.equalsIgnoreCase(sha)) {
            throw new IllegalStateException("unexpected XEX SHA-256: " + sha);
        }
        byte[] pdata = readQualifiedPdata();

        Disassembler disassembler =
            Disassembler.getDisassembler(currentProgram, monitor, null);
        Set<Address> returningCallees = new HashSet<>();
        int totalCleared = 0;
        int totalUnflagged = 0;

        for (int round = 1; round <= MAX_ROUNDS; ++round) {
            if (monitor.isCancelled()) {
                break;
            }
            Set<Address> fallThroughs = new LinkedHashSet<>();
            Set<Function> affected = new LinkedHashSet<>();
            int cleared = 0;
            int unflagged = 0;

            for (int offset = 0; offset + 8 <= pdata.length; offset += 8) {
                long begin = beU32(pdata, offset);
                long packed = beU32(pdata, offset + 4);
                if (begin < CODE_MIN || begin > CODE_MAX) {
                    continue;
                }
                long length = ((packed >>> 8) & 0x3fffffL) * 4;
                if (length <= 4) {
                    continue;
                }
                long end = Math.min(begin + length, CODE_MAX + 1);

                for (long cursor = begin; cursor < end; cursor += 4) {
                    Address address = toAddr(cursor);
                    Instruction instruction = getInstructionAt(address);
                    if (instruction == null) {
                        continue;
                    }
                    if (!instruction.getFlowType().isCall()
                            || instruction.getFallThrough() != null) {
                        continue;
                    }
                    // The linker records more function after this call, so the
                    // call returns. Anything else contradicts .pdata.
                    if (cursor + 4 >= end) {
                        continue;
                    }

                    // Clear the callee's flag first: leaving it set makes Ghidra
                    // re-derive the same terminal flow on the next analysis.
                    for (Address target : instruction.getFlows()) {
                        Function callee = getFunctionAt(target);
                        if (callee != null && callee.hasNoReturn()
                                && returningCallees.add(target)) {
                            callee.setNoReturn(false);
                            unflagged++;
                            println("AC6_UNFLAGGED_NORETURN " + target
                                + " " + callee.getName()
                                + " witness=" + address);
                        }
                    }

                    instruction.setFlowOverride(FlowOverride.NONE);
                    Address fallThrough = instruction.getFallThrough();
                    if (fallThrough == null) {
                        continue;
                    }
                    cleared++;
                    fallThroughs.add(fallThrough);
                    Function owner = getFunctionContaining(address);
                    if (owner != null) {
                        affected.add(owner);
                    }
                }
            }

            for (Address fallThrough : fallThroughs) {
                if (getInstructionAt(fallThrough) == null) {
                    disassembler.disassemble(fallThrough, null);
                }
            }
            int grown = 0;
            for (Function function : affected) {
                long before = function.getBody().getNumAddresses();
                if (CreateFunctionCmd.fixupFunctionBody(currentProgram, function, monitor)
                        && function.getBody().getNumAddresses() > before) {
                    grown++;
                }
            }

            totalCleared += cleared;
            totalUnflagged += unflagged;
            println(String.format(
                "AC6_ROUND %d cleared=%d unflagged_callees=%d bodies_grown=%d",
                round, cleared, unflagged, grown));
            if (cleared == 0) {
                break;
            }
        }

        println("AC6_PDATA_FLOW_SUMMARY cleared=" + totalCleared
            + " unflagged_callees=" + totalUnflagged);
    }
}
