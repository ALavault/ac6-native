// Export one bounded initialized memory range for a deterministic local tool.
// This does not modify the Ghidra project. Do not export retail data into Git.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;

public class ExportMemoryRange extends GhidraScript {
    private static final int MAX_BYTES = 64 * 1024 * 1024;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 3) {
            throw new IllegalArgumentException(
                "usage: ExportMemoryRange <address> <length> <output>");
        }
        Address address = toAddr(Long.decode(args[0]));
        int length = Integer.decode(args[1]);
        if (length <= 0 || length > MAX_BYTES) {
            throw new IllegalArgumentException("length is outside the bounded range");
        }
        byte[] bytes = new byte[length];
        int read = currentProgram.getMemory().getBytes(address, bytes);
        if (read != length) {
            throw new IllegalArgumentException("range crosses unavailable memory");
        }
        Path output = Path.of(args[2]).toAbsolutePath().normalize();
        Files.write(output, bytes, StandardOpenOption.CREATE_NEW,
                    StandardOpenOption.WRITE);
        println("exported=" + output + " address=" + address +
                " bytes=" + length);
    }
}
