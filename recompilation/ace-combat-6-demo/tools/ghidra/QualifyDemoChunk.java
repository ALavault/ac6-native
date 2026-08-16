// Qualify a bounded inner chunk after inspecting it in the demo-specific
// Ghidra project. The writable project is disposable; only the JSON record is
// consumed by the build script.
import ghidra.app.script.GhidraScript;
import ghidra.app.util.PseudoDisassembler;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.SourceType;

import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;

public class QualifyDemoChunk extends GhidraScript {
  private static final String XEX_SHA256 =
      "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8";

  private static String address(long value) {
    return String.format("0x%08X", value);
  }

  private static String quote(String value) {
    return "\"" + value.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
  }

  private String sha256(Address start, int bytes) throws Exception {
    byte[] data = new byte[bytes];
    currentProgram.getMemory().getBytes(start, data);
    byte[] digest = MessageDigest.getInstance("SHA-256").digest(data);
    StringBuilder result = new StringBuilder();
    for (byte value : digest) {
      result.append(String.format("%02x", value & 0xff));
    }
    return result.toString();
  }

  private Address findDirectCaller(Address target) throws Exception {
    var text = currentProgram.getMemory().getBlock(".text");
    if (text == null) {
      throw new IllegalStateException("missing executable .text block");
    }
    for (Address candidate = text.getStart();
         candidate.add(3).compareTo(text.getEnd()) <= 0;
         candidate = candidate.add(4)) {
      int word = currentProgram.getMemory().getInt(candidate);
      // PowerPC op=18: b/bl, with AA=0 and LK=1 for a relative direct call.
      if ((word >>> 26) != 18 || (word & 0x3) != 1) {
        continue;
      }
      int displacement = word & 0x03fffffc;
      if ((displacement & 0x02000000) != 0) {
        displacement |= 0xfc000000;
      }
      Address destination = toAddr((candidate.getOffset() + displacement) & 0xffffffffL);
      if (destination.equals(target)) {
        return candidate;
      }
    }
    return null;
  }

  private Address findFollowingPdataOwner(Address after) throws Exception {
    var pdata = currentProgram.getMemory().getBlock(".pdata");
    if (pdata == null) {
      throw new IllegalStateException("missing .pdata block");
    }
    for (Address row = pdata.getStart();
         row.add(7).compareTo(pdata.getEnd()) <= 0;
         row = row.add(8)) {
      long owner = Integer.toUnsignedLong(currentProgram.getMemory().getInt(row));
      long packed = Integer.toUnsignedLong(currentProgram.getMemory().getInt(row.add(4)));
      long size = ((packed >>> 8) & 0x3fffffL) * 4;
      if (owner > after.getOffset() && size != 0) {
        Address ownerAddress = toAddr(owner);
        var ownerBlock = currentProgram.getMemory().getBlock(ownerAddress);
        if (ownerBlock != null && ".text".equals(ownerBlock.getName())) {
          return ownerAddress;
        }
      }
    }
    return null;
  }

  private Instruction instructionAt(PseudoDisassembler pseudo, Address address) {
    Instruction instruction = getInstructionAt(address);
    if (instruction != null) {
      return instruction;
    }
    try {
      return pseudo.disassemble(address);
    } catch (Exception ignored) {
      return null;
    }
  }

  @Override
  public void run() throws Exception {
    if (!"ace-combat-6-demo".equals(state.getProject().getName()) ||
        !"PowerPC:BE:64:Xenon".equals(
            currentProgram.getLanguageID().toString()) ||
        !"Default.xex".equals(currentProgram.getName()) ||
        !XEX_SHA256.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
      throw new IllegalStateException("wrong qualified demo project or XEX");
    }
    String[] arguments = getScriptArgs();
    if (arguments.length != 3 && arguments.length != 4) {
      throw new IllegalArgumentException("expected start, last-instruction, output[, indirect-dispatch]");
    }
    boolean boundedEntry = arguments.length == 4 &&
        ("indirect-dispatch".equals(arguments[3]) || "bounded-entry".equals(arguments[3]));
    long startValue = Long.decode(arguments[0]) & 0xffffffffL;
    long lastValue = Long.decode(arguments[1]) & 0xffffffffL;
    if ((startValue & 3) != 0 || (lastValue & 3) != 0 || lastValue < startValue) {
      throw new IllegalArgumentException("chunk bounds must be aligned and ordered");
    }
    Address start = toAddr(startValue);
    Address last = toAddr(lastValue);
    int bytes = (int) (lastValue - startValue + 4);
    Function containing = getFunctionContaining(start);
    if (containing != null && !containing.getEntryPoint().equals(start)) {
      throw new IllegalStateException("chunk starts inside " + containing.getEntryPoint());
    }
    Disassembler disassembler = Disassembler.getDisassembler(currentProgram, monitor, null);
    PseudoDisassembler pseudo = new PseudoDisassembler(currentProgram);
    for (int offset = 0; offset < bytes; offset += 4) {
      Address address = start.add(offset);
      if (instructionAt(pseudo, address) == null) {
        disassembler.disassemble(address, new AddressSet(start, last.add(3)));
      }
      if (instructionAt(pseudo, address) == null) {
        // Ghidra leaves a zero PPC branch-delay-slot undefined when the
        // preceding branch has no fallthrough.  Accept only that exact
        // architectural nop; every other undecoded word remains fatal.
        Instruction previous = offset >= 4 ? instructionAt(pseudo, address.subtract(4)) : null;
        if (currentProgram.getMemory().getInt(address) == 0 && previous != null &&
            previous.getFlowType().isJump() && !previous.getFlowType().hasFallthrough()) {
          continue;
        }
        throw new IllegalStateException("undecoded instruction at " + address);
      }
    }

    Function function = getFunctionAt(start);
    if (function != null) {
      function.setBody(new AddressSet(start, last.add(3)));
      function.setName("DemoQualifiedChunk_" + address(startValue), SourceType.USER_DEFINED);
    }

    boolean foundReturn = false;
    Address directCaller = findDirectCaller(start);
    for (int offset = 0; offset < bytes; offset += 4) {
      Instruction instruction = instructionAt(pseudo, start.add(offset));
      if (instruction == null) {
        continue;
      }
      boolean boundedTailJump =
          instruction.getAddress().equals(last) &&
          instruction.getFlowType().isJump() &&
          !instruction.getFlowType().hasFallthrough();
      if (instruction.getFlowType().isTerminal() || instruction.getFlowType().isCall() ||
          boundedTailJump) {
        foundReturn = true;
      }
    }
    if (!foundReturn) {
      throw new IllegalStateException("chunk has no terminal flow at " + last);
    }
    if (directCaller == null && !boundedEntry) {
      throw new IllegalStateException("chunk has no direct caller: " + start);
    }
    if (boundedEntry) {
      Instruction lastInstruction = instructionAt(pseudo, last);
      Instruction previous = instructionAt(pseudo, start.subtract(4));
      boolean boundedTerminal = lastInstruction != null &&
          (lastInstruction.getFlowType().isTerminal() ||
           (lastInstruction.getFlowType().isJump() &&
            !lastInstruction.getFlowType().hasFallthrough()));
      boolean boundedPredecessor = previous == null ?
          currentProgram.getMemory().getInt(start.subtract(4)) == 0 :
          (previous.getFlowType().isTerminal() ||
           (previous.getFlowType().isJump() && !previous.getFlowType().hasFallthrough()));
      if (!boundedTerminal || !boundedPredecessor) {
        throw new IllegalStateException("indirect dispatch chunk lacks bounded computed jump/padding: " + start);
      }
    }
    Function nextFunction = getFunctionAt(last.add(4));
    Address followingOwner = nextFunction == null ? findFollowingPdataOwner(last) :
        nextFunction.getEntryPoint();
    if (followingOwner == null || followingOwner.equals(start)) {
      throw new IllegalStateException("callable chunk has no following owner boundary: " + last);
    }

    String output = arguments[2];
    try (BufferedWriter writer = Files.newBufferedWriter(
        Path.of(output), StandardCharsets.UTF_8, StandardOpenOption.CREATE,
        StandardOpenOption.TRUNCATE_EXISTING, StandardOpenOption.WRITE)) {
      writer.write("{\n");
      writer.write("  \"schema\": \"ac6-demo-ghidra-chunk-evidence/v2\",\n");
      writer.write("  \"target_id\": \"ac6-demo-xbox360-pal\",\n");
      writer.write("  \"project_path\": \"ghidra-projects/ace-combat-6-demo\",\n");
      writer.write("  \"project\": \"ace-combat-6-demo\",\n");
      writer.write("  \"program\": \"Default.xex\",\n");
      writer.write("  \"module\": \"Default.xex\",\n");
      writer.write("  \"language\": \"PowerPC:BE:64:Xenon\",\n");
      writer.write("  \"xex_sha256\": " + quote(XEX_SHA256) + ",\n");
      writer.write("  \"direct_caller\": " +
          quote(directCaller == null ? "-" : address(directCaller.getOffset())) + ",\n");
      writer.write("  \"following_owner\": " + quote(address(followingOwner.getOffset())) + ",\n");
      writer.write("  \"chunks\": [{\"address\": " + quote(address(startValue)) +
          ", \"size\": " + quote(address(bytes)) +
          ", \"name\": " + quote(function == null ?
              "DemoQualifiedChunk_" + address(startValue) : function.getName()) +
          ", \"byte_sha256\": " + quote(sha256(start, bytes)) + "}]\n");
      writer.write("}\n");
    }
    println("AC6_DEMO_CHUNK_PASS entry=" + start + " size=" + bytes +
        " direct_caller=" + directCaller + " following_owner=" + followingOwner +
        " output=" + output);
  }
}
