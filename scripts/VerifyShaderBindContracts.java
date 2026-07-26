// Read-only assertions for the two AC6 PAL shader-object device bindings.
// Stage names are a structural cross-match: +0x3190 has the vertex-specific
// +0x368 metadata block, while +0x318c uses the compact pixel-shader block.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyShaderBindContracts extends GhidraScript {
    private int assertions;

    private void require(long value, String mnemonic, String fragment)
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
        println("AC6_SHADER_BIND_CONTRACT=" + address + " " + instruction);
    }

    @Override
    public void run() throws Exception {
        // Compact shader object: r3=device, r4=new object, committed at +0x318c.
        require(0x821de2d4L, "or", "r30,r3,r3");
        require(0x821de2d8L, "or", "r29,r4,r4");
        require(0x821de2dcL, "lwz", "r31,0x318c(r30)");
        require(0x821de34cL, "stw", "r29,0x318c(r30)");
        require(0x821de350L, "cmplwi", "r29,0x0");
        require(0x821de36cL, "addi", "r11,r29,0x28");

        // Vertex-layout shader object: same ABI, committed at +0x3190. Its
        // state compiler starts from the object-specific +0x368 block.
        require(0x821de5ccL, "or", "r29,r4,r4");
        require(0x821de5d0L, "or", "r30,r3,r3");
        require(0x821de5e8L, "lwz", "r31,0x3190(r30)");
        require(0x821de660L, "stw", "r29,0x3190(r30)");
        require(0x821de65cL, "cmplwi", "r29,0x0");
        require(0x821de670L, "addic.", "r11,r29,0x368");

        // A common owner publishes the paired objects through the two setters
        // and the device reset path passes null to both.
        require(0x82350334L, "lwz", "r4,0x18(r31)");
        require(0x82350340L, "bl", "0x821de5c0");
        require(0x82350348L, "lwz", "r4,0x1c(r31)");
        require(0x8235034cL, "bl", "0x821de2c8");
        require(0x8233e50cL, "li", "r4,0x0");
        require(0x8233e514L, "bl", "0x821de5c0");
        require(0x8233e518L, "li", "r4,0x0");
        require(0x8233e520L, "bl", "0x821de2c8");

        // The common draw-state compiler consumes both committed fields before
        // building shader command packets.
        require(0x821ed214L, "lwz", "r31,0x3190(r30)");
        require(0x821ed21cL, "lwz", "r29,0x318c(r30)");
        require(0x821ed248L, "bne", "0x821ed2ec");
        require(0x821ed2ecL, "lwz", "r10,0x40(r29)");
        require(0x821ed36cL, "lwz", "r10,0x40(r29)");

        println("AC6_SHADER_BIND_ASSERTIONS=" + assertions);
    }
}
