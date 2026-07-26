import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;

public class DumpBytes extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException("usage: DumpBytes <address> <length>");
        }
        Address address = toAddr(Long.decode(args[0]));
        int length = Integer.decode(args[1]);
        byte[] bytes = new byte[length];
        currentProgram.getMemory().getBytes(address, bytes);
        for (int row = 0; row < length; row += 16) {
            StringBuilder line = new StringBuilder(address.add(row).toString());
            int end = Math.min(length, row + 16);
            for (int index = row; index < end; ++index) {
                line.append(String.format(" %02x", bytes[index] & 0xff));
            }
            println(line.toString());
        }
    }
}
