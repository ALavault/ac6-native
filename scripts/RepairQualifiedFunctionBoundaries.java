// Repair exact PAL function boundaries after byte-level qualification.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.FlowOverride;
import ghidra.program.model.symbol.SourceType;
import java.security.MessageDigest;

public class RepairQualifiedFunctionBoundaries extends GhidraScript {
    private void markReturningHelper(String addressText, String name) throws Exception {
        Function helper = getFunctionAt(toAddr(addressText));
        if (helper == null) {
            throw new IllegalStateException("missing qualified save helper " + addressText);
        }
        helper.setNoReturn(false);
        helper.setName(name, SourceType.USER_DEFINED);
        println(helper.getEntryPoint() + " " + helper.getName() +
                " noreturn=" + helper.hasNoReturn());
    }

    private void clearCallTerminator(String addressText) throws Exception {
        Instruction call = getInstructionAt(toAddr(addressText));
        if (call == null) {
            throw new IllegalStateException("missing qualified save-helper call " + addressText);
        }
        call.setFlowOverride(FlowOverride.NONE);
        println(call.getAddress() + " flow=" + call.getFlowType() +
                " fallthrough=" + call.getFallThrough());
    }

    private void qualifyRange(String startText, String endText, String expectedSha256)
            throws Exception {
        Address start = toAddr(startText);
        Address end = toAddr(endText);
        // The end argument names the last instruction start. Include its
        // complete four-byte PPC word in the byte qualification.
        long length = end.subtract(start) + 4;
        if (length <= 0 || length > Integer.MAX_VALUE) {
            throw new IllegalStateException("invalid qualified range " + start + ".." + end);
        }
        byte[] bytes = new byte[(int) length];
        // Memory.getBytes(Address, byte[]) treats undefined instructions as
        // zeroes in this imported XEX. Read individual bytes so the guard
        // covers the actual loader bytes even before a function body exists.
        for (int i = 0; i < bytes.length; ++i) {
            bytes[i] = currentProgram.getMemory().getByte(start.add(i));
        }
        byte[] digest = MessageDigest.getInstance("SHA-256").digest(bytes);
        StringBuilder actual = new StringBuilder(64);
        for (byte value : digest) {
            actual.append(String.format("%02x", value & 0xff));
        }
        if (!expectedSha256.equalsIgnoreCase(actual.toString())) {
            throw new IllegalStateException("qualified range SHA-256 mismatch for " + start + ".."
                    + end + ": expected " + expectedSha256 + ", got " + actual);
        }
        println("qualified bytes " + start + ".." + end + " sha256=" + actual);
    }

    private void repair(String startText, String endText, String name) throws Exception {
        Address start = toAddr(startText);
        Address end = toAddr(endText);
        AddressSet body = new AddressSet(start, end);
        // The imported XEX contains only the ABI prologue for several of these
        // entries.  Seed disassembly at every still-undefined PPC word;
        // repairing a function boundary without defining its instructions
        // leaves the bridge with a plausible name and a two-instruction
        // decompilation.  The loader's undefined ranges need an unrestricted
        // seed; the explicit body below remains the byte-qualified boundary.
        Disassembler disassembler = Disassembler.getDisassembler(currentProgram, monitor, null);
        for (long offset = 0; offset <= end.subtract(start); offset += 4) {
            Address cursor = start.add(offset);
            if (getInstructionAt(cursor) == null) {
                disassembler.disassemble(cursor, null);
            }
        }

        Function function = getFunctionAt(start);
        if (function == null) {
            function = createFunction(start, name);
        }
        if (function == null) {
            throw new IllegalStateException("could not create function at " + start);
        }

        function.setBody(body);
        function.setName(name, SourceType.USER_DEFINED);
        println(start + " " + function.getName() + " " + function.getBody());
    }

    @Override
    protected void run() throws Exception {
        String sha = currentProgram.getExecutableSHA256();
        if (!"acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde".equalsIgnoreCase(sha)) {
            throw new IllegalStateException("unexpected XEX SHA-256: " + sha);
        }
        // The imported XEX marks these ABI save helpers as no-return thunks.
        // They return through the paired restgpr helpers, so that annotation
        // truncates every generated body whose prologue saves r14/r22/r29.
        markReturningHelper("0x82382EC0", "__savegprlr_14");
        markReturningHelper("0x82382EE0", "__savegprlr_22");
        markReturningHelper("0x82382EFC", "__savegprlr_29");
        markReturningHelper("0x82382ED4", "__savegprlr_19");
        Instruction saveCall = getInstructionAt(toAddr("0x820F6334"));
        if (saveCall == null) {
            throw new IllegalStateException("missing SendMsgV save-helper call");
        }
        saveCall.setFlowOverride(FlowOverride.NONE);
        println(saveCall.getAddress() + " flow=" + saveCall.getFlowType() +
                " fallthrough=" + saveCall.getFallThrough());
        clearCallTerminator("0x8218C23C");
        clearCallTerminator("0x821C37E4");
        clearCallTerminator("0x821C3BEC");
        clearCallTerminator("0x821C4FA4");
        clearCallTerminator("0x821C525C");
        clearCallTerminator("0x821C56FC");
        repair("0x820F62B0", "0x820F6328", "ParseSwgMessageId_820F62B0");
        repair("0x820F6330", "0x820F63CC", "SendMsgV_820F6330");
        repair("0x8214D390", "0x8214D3B0", "CSelectAircraftManager_OnSwgMessage_8214D390");
        // These two generated entries are long executable bodies in the PAL XEX.
        // The original import retained only their two-instruction prologues, so
        // qualify both byte ranges before repairing their function boundaries.
        qualifyRange("0x8218C238", "0x8218CCA4",
                "f4f7e8f2382789a084bbf648bfb7884cbc87d7c8242f1efddcd7b01cacf289d4");
        repair("0x8218C238", "0x8218CCA4", "CampaignSaveOuter_8218C238");
        qualifyRange("0x821C37E0", "0x821C3BE0",
                "4903a839277c318929347d4790840deb82000f5541d1e5bf575f63a973642120");
        repair("0x821C37E0", "0x821C3BE0", "CampaignSaveDialog_821C37E0");
        // The selector/create helpers are adjacent long bodies in the same
        // qualified PAL XEX.  The imported project retained only their
        // prologues; these ranges are byte-qualified before their boundaries
        // are repaired.  Their end addresses are the final PPC instruction
        // starts immediately before the next generated entry point.
        qualifyRange("0x821C3BE8", "0x821C402C",
                "0bcfd5aaa6e05a42b6ee68c39d1598c211c9f0630b8499aa6e24b0c1c9efd605");
        repair("0x821C3BE8", "0x821C402C", "FileSelector_821C3BE8");
        qualifyRange("0x821C4FA0", "0x821C5254",
                "e07514643c0f9d24d4197b0613298a65757406436b424452dbdacf6e22cb11c5");
        repair("0x821C4FA0", "0x821C5254", "FileCreateDialog_821C4FA0");
        qualifyRange("0x821C5258", "0x821C56F4",
                "fc64454a236a7892efbadf43250ab6f5a2fd3c15368cc2fd4ce9329f272dfbaf");
        repair("0x821C5258", "0x821C56F4", "FileCreateTask_821C5258");
        qualifyRange("0x821C56F8", "0x821C59AC",
                "2340a5aab20b3603c13596b173b9e69b9f7358b73a47ee9482d0480247804f49");
        repair("0x821C56F8", "0x821C59AC", "FileCreateState_821C56F8");
    }
}
