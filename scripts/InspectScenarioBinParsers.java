// Read-only inspection of the retail scenario parser family (*Bin::read /
// *Bin::getReadBuffSize), whose class names come from the self-describing error
// strings in the XEX. Reports body size, instruction count, decompiled length
// and the split-address materializations each parser performs, so the analysis
// gap that hid this family can be measured before and after repair.
// Run with -readOnly -noanalysis.
// @category AC6

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.scalar.Scalar;

public class InspectScenarioBinParsers extends GhidraScript {

    private static final String QUALIFIED_XEX_SHA256 =
        "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";

    private static final Object[][] PARSERS = {
        {0x82309620L, "SubMisTblBin::getReadBuffSize"},
        {0x82309758L, "SubMisTblBin::read"},
        {0x8232c7e0L, "SubMisBin::getReadBuffSize"},
        {0x8232c8a8L, "SubMisBin::read"},
        {0x8232f4d0L, "SetBin::getReadBuffSize"},
        {0x8232f5f8L, "SetBin::read"},
        {0x8232ff78L, "ObjBin::getReadBuffSize"},
        {0x82330158L, "ObjBin::read"},
        {0x82330540L, "ActBin::getReadBuffSize"},
        {0x82330688L, "ActBin::read"},
        {0x823310e8L, "OrderBin::getReadBuffSize"},
        {0x82331208L, "OrderBin::read"},
        {0x823316a0L, "ManeuverBin::getReadBuffSize"},
        {0x82331808L, "ManeuverBin::read"},
        {0x82331bb0L, "ComTblBin::getReadBuffSize"},
        {0x82331c10L, "ComTblBin::read"},
        {0x82331d98L, "ComBin::read"},
    };

    private DecompInterface decompiler;

    private static long word(Instruction instruction) throws Exception {
        byte[] bytes = instruction.getBytes();
        return ((long) (bytes[0] & 0xff) << 24) | ((long) (bytes[1] & 0xff) << 16)
             | ((long) (bytes[2] & 0xff) << 8)  | (long) (bytes[3] & 0xff);
    }

    private static int signed16(long value) {
        int result = (int) (value & 0xffffL);
        return result >= 0x8000 ? result - 0x10000 : result;
    }

    /**
     * Reports every 32-bit constant this function materializes into the .rdata
     * string region, whether or not a reference exists for it.
     *
     * The pairing must track the destination register of the lis: one lis
     * commonly feeds several addi. At 0x823096C0 the compiler emits
     * `lis r11,0x8201`, then an unrelated `addi r29,r27,0x4`, then
     * `addi r24,r11,-0xa58` -> 0x8200F5A8. Pairing a lis with the next addi
     * regardless of register reports the wrong constant and hides the real one.
     */
    private void reportStringMaterializations(Function function) throws Exception {
        // high half currently held per register, or null if unknown.
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

            // lis rD,SI is addis rD,r0,SI
            if (opcode == 15 && ((encoding >>> 16) & 31) == 0) {
                high[(int) ((encoding >>> 21) & 31)] =
                    (((long) signed16(encoding)) << 16) & 0xffffffffL;
                continue;
            }

            boolean addi = opcode == 14;
            boolean ori = opcode == 24;
            if (!addi && !ori) {
                continue;
            }
            int source = (int) (addi ? (encoding >>> 16) & 31 : (encoding >>> 21) & 31);
            int destination = (int) (addi ? (encoding >>> 21) & 31 : (encoding >>> 16) & 31);
            Long base = high[source];
            if (base == null) {
                high[destination] = null;
                continue;
            }
            long value = addi
                ? (base + signed16(encoding)) & 0xffffffffL
                : (base | (encoding & 0xffffL)) & 0xffffffffL;
            high[destination] = null;
            if (value < 0x82000000L || value >= 0x82090000L) {
                continue;   // outside .rdata
            }
            Address target = toAddr(value);
            String text = "";
            if (getDataAt(target) != null && getDataAt(target).hasStringValue()) {
                text = String.valueOf(getDataAt(target).getValue());
            }
            println(String.format("    MATERIALIZES 0x%08X at %s refs=%d %s",
                value, instruction.getAddress(),
                getReferencesTo(target).length, text));
        }
    }

    private void inspect(long entryValue, String label) throws Exception {
        Address entry = toAddr(entryValue);
        Function function = getFunctionAt(entry);
        if (function == null) {
            println(String.format("AC6_PARSER %s %-32s NO_FUNCTION", entry, label));
            return;
        }

        int instructions = 0;
        InstructionIterator iterator =
            currentProgram.getListing().getInstructions(function.getBody(), true);
        while (iterator.hasNext()) {
            iterator.next();
            instructions++;
        }

        DecompileResults results = decompiler.decompileFunction(function, 60, monitor);
        int decompiledLength = 0;
        if (results != null && results.getDecompiledFunction() != null) {
            decompiledLength = results.getDecompiledFunction().getC().length();
        }

        println(String.format("AC6_PARSER %s %-32s body=%d instructions=%d decompiled_chars=%d",
            entry, label, function.getBody().getNumAddresses(), instructions, decompiledLength));
        reportStringMaterializations(function);
    }

    @Override
    protected void run() throws Exception {
        String sha = currentProgram.getExecutableSHA256();
        if (!QUALIFIED_XEX_SHA256.equalsIgnoreCase(sha)) {
            throw new IllegalStateException("unexpected XEX SHA-256: " + sha);
        }
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try {
            for (Object[] parser : PARSERS) {
                inspect((Long) parser[0], (String) parser[1]);
            }
        } finally {
            decompiler.dispose();
        }
    }
}
