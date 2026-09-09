// Read the raw float64 values at _UNK_8205474c and _UNK_82069c20, the two
// real-time delay thresholds used by Function_821C5268's state machine
// (dVar8/dVar7 in the decompile), to quantify how long the save-screen
// state machine's time-gated waits actually are.
// @category AC6
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;

public class Ac6SaveTimingConstants extends GhidraScript {
    @Override
    protected void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        long[] addrs = {0x8205474cL, 0x82069c20L};
        for (long a : addrs) {
            Address addr = toAddr(a);
            byte[] bytes = new byte[8];
            mem.getBytes(addr, bytes);
            long bits = 0;
            for (int i = 0; i < 8; i++) {
                bits = (bits << 8) | (bytes[i] & 0xffL);
            }
            double value = Double.longBitsToDouble(bits);
            println("AC6_TIMING addr=" + addr + " bits=0x" + Long.toHexString(bits) + " double=" + value);
        }
    }
}
