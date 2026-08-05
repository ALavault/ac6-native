// Read-only qualification of the PAL campaign render-selector boundary.
// This script never creates or edits functions and is tied to the canonical
// default.xex Ghidra program.

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import java.security.MessageDigest;

public class QualifyCampaignRenderSelector extends GhidraScript {
    private static final String XEX_SHA256 =
        "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";

    private static String hex(byte[] bytes) {
        StringBuilder out = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) {
            out.append(String.format("%02x", value & 0xff));
        }
        return out.toString();
    }

    private String bytes(String addressText, int length) throws Exception {
        byte[] data = new byte[length];
        currentProgram.getMemory().getBytes(toAddr(addressText), data);
        return hex(data);
    }

    private String sha256(String addressText, int length) throws Exception {
        byte[] data = new byte[length];
        currentProgram.getMemory().getBytes(toAddr(addressText), data);
        return hex(MessageDigest.getInstance("SHA-256").digest(data));
    }

    private void qualify(String label, String addressText, int length) throws Exception {
        Address address = toAddr(addressText);
        Function function = getFunctionContaining(address);
        Instruction instruction = currentProgram.getListing().getInstructionAt(address);
        if (instruction == null) {
            Disassembler.getDisassembler(currentProgram, monitor, null)
                .disassemble(address, null);
            instruction = currentProgram.getListing().getInstructionAt(address);
            function = getFunctionContaining(address);
        }
        println(label + " address=" + address + " length=" + length +
                " bytes=" + bytes(addressText, length) +
                " sha256=" + sha256(addressText, length));
        println("  function=" + (function == null ? "<none>" :
                function.getEntryPoint() + " " + function.getName() +
                " body=" + function.getBody()));
        println("  instruction=" + (instruction == null ? "<none>" : instruction));
    }

    private void qualifyViewWriter(String addressText) throws Exception {
        qualify("view_writer_candidate", addressText, 16);
    }

    @Override
    protected void run() throws Exception {
        String sha = currentProgram.getExecutableSHA256();
        if (!XEX_SHA256.equalsIgnoreCase(sha)) {
            throw new IllegalStateException("unexpected XEX SHA-256: " + sha);
        }
        println("program=" + currentProgram.getName() + " xex_sha256=" + sha);
        qualify("campaign_entry", "0x821A16B8", 16);
        qualify("mask_load_fingerprint", "0x821A1704", 4);
        qualify("mask_test_fingerprint", "0x821A1884", 16);
        qualify("context_prepare", "0x821A0DFC", 40);
        qualify("up_hud_marker", "0x8226DF00", 32);
        qualify("up_hud_call", "0x8226DF1C", 4);
        String[] viewWriters = {
            "0x822E5310", "0x822E5454", "0x822E7218", "0x822E74F8",
            "0x822E76E4", "0x822E78FC", "0x822E89A8"
        };
        for (String address : viewWriters) {
            qualifyViewWriter(address);
        }
    }
}
