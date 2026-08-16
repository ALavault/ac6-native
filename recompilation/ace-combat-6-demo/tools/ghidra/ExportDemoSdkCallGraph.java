import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.address.AddressRangeIterator;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;

/**
 * Export a deterministic, read-only call graph rooted at every qualified demo
 * XEX import.  Run with -readOnly -noanalysis.  The script deliberately does
 * not attribute bctr/bctrl sites to an import without separate qualified
 * vtable/slot evidence; such sites in graph-owned functions are exported as
 * unresolved instead.
 */
public class ExportDemoSdkCallGraph extends GhidraScript {
  private static final String SCHEMA = "ac6-demo-sdk-callgraph/v1";
  private static final String TARGET_ID = "ac6-demo-xbox360-pal";
  private static final String PROJECT = "ace-combat-6-demo";
  private static final String PROJECT_PATH = "ghidra-projects/ace-combat-6-demo";
  private static final String PROGRAM = "Default.xex";
  private static final String LANGUAGE = "PowerPC:BE:64:Xenon";
  private static final String COMPILER_SPEC = "default";
  private static final String XEX_SHA256 =
      "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8";
  private static final long IMAGE_BASE = 0x82000000L;
  private static final long TEXT_START = 0x82090000L;
  private static final long TEXT_SIZE = 0x002e67c4L;
  private static final String TEXT_SHA256 =
      "6531a539783328976898a3df802ffcbcff40a4ad13f643836bedbdb137dd86f6";
  private static final long PDATA_START = 0x82077200L;
  private static final long PDATA_SIZE = 0x00010438L;
  private static final String PDATA_SHA256 =
      "82c68b78f3256dd0c2bdd0df40e97daf6f3cf6dd1e162d5ddee4a47d1d14e50b";
  private static final int EXPECTED_IMPORTS = 238;
  private static final int EXPECTED_CALLABLES = 228;
  private static final int EXPECTED_VARIABLES = 10;
  private static final int EXPECTED_XAM_IMPORTS = 87;
  private static final int EXPECTED_XBOXKRNL_IMPORTS = 151;
  private static final int MAX_WRAPPER_DEPTH = 4;
  private static final int MAX_WRAPPER_BYTES = 0x100;
  private static final int MAX_WRAPPER_INSTRUCTIONS = 32;
  private static final int MAX_PROVENANCE_SCAN = 64;
  private static final int MAX_PROVENANCE_CHAIN = 5;

  private static final Set<String> XAM_NAMES = Collections.unmodifiableSet(
      new HashSet<>(Arrays.asList((
          "NetDll_WSAStartup NetDll_WSACleanup NetDll_socket NetDll_closesocket " +
          "NetDll_shutdown NetDll_ioctlsocket NetDll_setsockopt NetDll_getsockopt " +
          "NetDll_getsockname NetDll_bind NetDll_connect NetDll_listen NetDll_accept " +
          "NetDll_select NetDll_recv NetDll_recvfrom NetDll_send NetDll_sendto " +
          "NetDll_WSAGetLastError NetDll___WSAFDIsSet NetDll_XNetStartup " +
          "NetDll_XNetCleanup NetDll_XNetRandom NetDll_XNetCreateKey " +
          "NetDll_XNetRegisterKey NetDll_XNetXnAddrToInAddr " +
          "NetDll_XNetInAddrToXnAddr NetDll_XNetQosListen NetDll_XNetQosLookup " +
          "NetDll_XNetQosServiceLookup NetDll_XNetQosRelease " +
          "NetDll_XNetGetTitleXnAddr XamInputGetCapabilities XamInputGetState " +
          "XamInputSetState XamInputGetKeystrokeEx XamLoaderLaunchTitle " +
          "XamLoaderSetLaunchData XamLoaderGetLaunchDataSize XamLoaderGetLaunchData " +
          "XamLoaderTerminateTitle XamTaskSchedule XamTaskCloseHandle XamTaskShouldExit " +
          "XamAlloc XamFree XMsgInProcessCall XMsgStartIORequest XMsgCancelIORequest " +
          "XamUserGetXUID XamUserGetName XamUserGetSigninState XamUserCheckPrivilege " +
          "XamUserAreUsersFriends XamUserReadProfileSettings XamEnumerate " +
          "XamContentCreateEx XamContentClose XamContentDelete XamContentCreateEnumerator " +
          "XamContentGetDeviceData XamContentSetThumbnail XamContentGetDeviceState " +
          "XamGetExecutionId XamGetSystemVersion XamNotifyCreateListener XNotifyGetNext " +
          "XNotifyPositionUI XamShowSigninUI XamShowPlayerReviewUI " +
          "XamShowDeviceSelectorUI XamShowGameInviteUI XamShowGamerCardUIForXUID " +
          "XamShowDirtyDiscErrorUI XamShowMessageBoxUIEx " +
          "XamUserCreateAchievementEnumerator XamUserCreateStatsEnumerator " +
          "XamVoiceCreate XamVoiceHeadsetPresent XamVoiceSubmitPacket XamVoiceClose " +
          "XamSessionCreateHandle XamSessionRefObjByHandle XGetAVPack XGetGameRegion " +
          "XGetLanguage XGetVideoMode").split(" "))));

  private final Map<String, OwnerNode> nodes = new TreeMap<>();
  private final Map<String, Map<String, Object>> edges = new TreeMap<>();
  private final Map<String, Map<String, Object>> unresolvedIndirect = new TreeMap<>();
  private final Map<String, List<Symbol>> symbolsByName = new HashMap<>();

  private static class ImportRecord {
    String module;
    String name;
    String ghidraName;
    int ordinal;
    Address slot;
    Address stub;
    int slotRaw;
    Integer stubRaw;
    String kind;
    final List<Site> direct = new ArrayList<>();
    final List<Site> nonCall = new ArrayList<>();
    final List<Site> rejectedIndirect = new ArrayList<>();
    final List<WrapperLink> wrappers = new ArrayList<>();
    final List<Site> wrapperFrontiers = new ArrayList<>();

    String id() {
      return "import:" + module + ":" + ordinal + ":" + name;
    }
  }

  private static class OwnerNode {
    String id;
    String kind;
    String name;
    Address entry;
    final List<long[]> ranges = new ArrayList<>();
    long size;
    String byteSha256;
    Function function;
  }

  private static class Site {
    Address address;
    String instruction;
    String instructionSha256;
    String referenceType;
    String kind;
    String lr;
    OwnerNode owner;
    String targetNodeId;
    Address targetAddress;
    List<Map<String, Object>> arguments = new ArrayList<>();

    String edgeId() {
      return "edge:" + address(address) + "->" + targetNodeId + ":" + kind;
    }
  }

  private static class WrapperDecision {
    boolean qualified;
    String kind;
    int bytes;
    int instructions;
    int matchingTargetEdges;
    int otherExternalEdges;
    int unresolvedIndirectEdges;
    String reason;
  }

  private static class WrapperLink {
    int depth;
    OwnerNode wrapper;
    Site edgeToTarget;
    WrapperDecision decision;
  }

  @Override
  public void run() throws Exception {
    String[] args = getScriptArgs();
    if (args.length != 3) {
      throw new IllegalArgumentException(
          "usage: ExportDemoSdkCallGraph.java <output.json> " + TARGET_ID +
          " read-only-noanalysis");
    }
    if (!TARGET_ID.equals(args[1])) {
      throw new IllegalStateException("target ID mismatch: " + args[1]);
    }
    if (!"read-only-noanalysis".equals(args[2])) {
      throw new IllegalStateException("invocation mode mismatch: " + args[2]);
    }
    qualifyProgram();
    indexSymbols();
    List<ImportRecord> imports = readImports();
    for (ImportRecord record : imports) {
      collectImportReferences(record);
      if (record.stub != null) {
        Set<String> visited = new HashSet<>();
        collectWrappers(record, record.stub, record.id(), 1, visited);
      }
    }
    collectUnresolvedIndirects();
    Map<String, Object> document = buildDocument(imports);
    String json = json(document, 0) + "\n";
    File output = new File(args[0]).getCanonicalFile();
    File parent = output.getParentFile();
    if (parent == null || !parent.isDirectory()) {
      throw new IllegalStateException("output parent does not exist: " + output);
    }
    try (BufferedWriter writer = new BufferedWriter(new FileWriter(output,
        StandardCharsets.UTF_8))) {
      writer.write(json);
    }
    println("AC6_DEMO_SDK_CALLGRAPH_PASS imports=" + imports.size() +
        " callables=" + countKind(imports, "callable") +
        " variables=" + countKind(imports, "variable") +
        " nodes=" + nodes.size() + " edges=" + edges.size() +
        " sha256=" + sha256(json.getBytes(StandardCharsets.UTF_8)));
  }

  private void qualifyProgram() throws Exception {
    if (state.getProject() == null || !PROJECT.equals(state.getProject().getName())) {
      throw new IllegalStateException("wrong canonical demo project");
    }
    if (!PROGRAM.equals(currentProgram.getName())) {
      throw new IllegalStateException("wrong demo program");
    }
    if (!LANGUAGE.equals(currentProgram.getLanguageID().toString())) {
      throw new IllegalStateException("wrong Xenon language");
    }
    if (!COMPILER_SPEC.equals(
        currentProgram.getCompilerSpec().getCompilerSpecID().toString())) {
      throw new IllegalStateException("wrong compiler spec");
    }
    if (!XEX_SHA256.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
      throw new IllegalStateException("wrong demo XEX SHA-256");
    }
    // XEXLoaderWV materializes absolute guest blocks but leaves Ghidra's
    // database image-base property at zero.  Qualify both facts explicitly.
    if ((currentProgram.getImageBase().getOffset() & 0xffffffffL) != 0 ||
        currentProgram.getMemory().getBlock(".text") == null ||
        (currentProgram.getMemory().getBlock(".text").getStart().getOffset() &
            0xffffffffL) != 0x82090000L) {
      throw new IllegalStateException("wrong demo loaded image layout");
    }
    if (XAM_NAMES.size() != EXPECTED_XAM_IMPORTS) {
      throw new IllegalStateException("embedded XAM roster is not 87 names");
    }
    if (!TEXT_SHA256.equals(hashRange(toAddr(TEXT_START), TEXT_SIZE)) ||
        !PDATA_SHA256.equals(hashRange(toAddr(PDATA_START), PDATA_SIZE))) {
      throw new IllegalStateException("canonical demo .text/.pdata byte identity mismatch");
    }
  }

  private void indexSymbols() {
    SymbolIterator iterator = currentProgram.getSymbolTable().getAllSymbols(true);
    while (iterator.hasNext()) {
      Symbol symbol = iterator.next();
      symbolsByName.computeIfAbsent(symbol.getName(), ignored -> new ArrayList<>())
          .add(symbol);
    }
    for (List<Symbol> symbols : symbolsByName.values()) {
      symbols.sort(Comparator.comparingLong(symbol ->
          symbol.getAddress().getOffset() & 0xffffffffL));
    }
  }

  private List<ImportRecord> readImports() throws Exception {
    List<Symbol> slots = new ArrayList<>();
    for (Map.Entry<String, List<Symbol>> entry : symbolsByName.entrySet()) {
      if (!entry.getKey().startsWith("__imp__")) {
        continue;
      }
      for (Symbol symbol : entry.getValue()) {
        if (symbol.getSource().toString().equals("IMPORTED")) {
          slots.add(symbol);
        }
      }
    }
    slots.sort(Comparator.comparingLong(symbol ->
        symbol.getAddress().getOffset() & 0xffffffffL));
    if (slots.size() != EXPECTED_IMPORTS) {
      throw new IllegalStateException("expected 238 imported slots, found " + slots.size());
    }

    Memory memory = currentProgram.getMemory();
    List<ImportRecord> result = new ArrayList<>();
    Set<String> unique = new HashSet<>();
    int xam = 0;
    int xboxkrnl = 0;
    for (Symbol slotSymbol : slots) {
      ImportRecord record = new ImportRecord();
      record.ghidraName = slotSymbol.getName().substring("__imp__".length());
      // XEXLoaderWV normalizes this one SDK spelling; retain the exact name
      // reported by the qualified XEX import directory in the export.
      record.name = record.ghidraName.equals("KiApcNormalRoutineNop") ?
          "KiApcNormalRoutineNop_0" : record.ghidraName;
      record.module = XAM_NAMES.contains(record.name) ? "xam.xex" : "xboxkrnl.exe";
      if (record.module.equals("xam.xex")) {
        xam++;
      } else {
        xboxkrnl++;
      }
      record.slot = slotSymbol.getAddress();
      record.slotRaw = memory.getInt(record.slot);
      record.ordinal = record.slotRaw & 0xffff;
      int expectedLibraryIndex = record.module.equals("xam.xex") ? 0 : 1;
      if (((record.slotRaw >>> 16) & 0xff) != expectedLibraryIndex ||
          (record.slotRaw & 0xff000000) != 0) {
        throw new IllegalStateException("non-canonical import slot word for " + record.name +
            " at " + address(record.slot) + " raw=" +
            String.format(Locale.ROOT, "0x%08X", record.slotRaw));
      }
      List<Address> matchingStubs = new ArrayList<>();
      List<Symbol> named = symbolsByName.getOrDefault(record.ghidraName,
          Collections.emptyList());
      for (Symbol candidate : named) {
        Address address = candidate.getAddress();
        if (!memory.contains(address)) {
          continue;
        }
        int raw;
        try {
          raw = memory.getInt(address);
        } catch (Exception ignored) {
          continue;
        }
        if (raw == (0x01000000 | record.slotRaw)) {
          matchingStubs.add(address);
        }
      }
      matchingStubs.sort(Comparator.comparingLong(address ->
          address.getOffset() & 0xffffffffL));
      if (matchingStubs.size() > 1) {
        throw new IllegalStateException("multiple import stubs for " + record.name);
      }
      if (matchingStubs.size() == 1) {
        record.kind = "callable";
        record.stub = matchingStubs.get(0);
        record.stubRaw = memory.getInt(record.stub);
      } else {
        record.kind = "variable";
      }
      if (!unique.add(record.module + ":" + record.ordinal + ":" + record.name)) {
        throw new IllegalStateException("duplicate import identity: " + record.name);
      }
      result.add(record);
    }
    result.sort(Comparator.comparing((ImportRecord record) -> record.module)
        .thenComparingInt(record -> record.ordinal).thenComparing(record -> record.name));
    if (xam != EXPECTED_XAM_IMPORTS || xboxkrnl != EXPECTED_XBOXKRNL_IMPORTS ||
        countKind(result, "callable") != EXPECTED_CALLABLES ||
        countKind(result, "variable") != EXPECTED_VARIABLES) {
      throw new IllegalStateException(String.format(Locale.ROOT,
          "import census mismatch xam=%d xboxkrnl=%d callable=%d variable=%d",
          xam, xboxkrnl, countKind(result, "callable"), countKind(result, "variable")));
    }
    return result;
  }

  private int countKind(List<ImportRecord> imports, String kind) {
    int count = 0;
    for (ImportRecord record : imports) {
      if (kind.equals(record.kind)) {
        count++;
      }
    }
    return count;
  }

  private void collectImportReferences(ImportRecord record) throws Exception {
    Address target = record.stub != null ? record.stub : record.slot;
    for (Site site : referencesTo(target, record.id(), record.stub != null)) {
      if (site.kind.equals("unresolved_indirect")) {
        record.rejectedIndirect.add(site);
      } else if (site.kind.equals("direct_call") || site.kind.equals("tail_call")) {
        record.direct.add(site);
        addEdge(site);
      } else {
        record.nonCall.add(site);
        addEdge(site);
      }
    }
    sortSites(record.direct);
    sortSites(record.nonCall);
    sortSites(record.rejectedIndirect);
  }

  private List<Site> referencesTo(Address target, String targetNodeId,
      boolean callableTarget) throws Exception {
    List<Site> result = new ArrayList<>();
    ReferenceIterator iterator = currentProgram.getReferenceManager().getReferencesTo(target);
    while (iterator.hasNext()) {
      Reference reference = iterator.next();
      Address from = reference.getFromAddress();
      Instruction instruction = currentProgram.getListing().getInstructionAt(from);
      if (instruction == null) {
        continue;
      }
      Site site = new Site();
      site.address = from;
      site.instruction = instruction.toString();
      site.instructionSha256 = hashRange(from, 4);
      site.referenceType = reference.getReferenceType().toString();
      site.targetNodeId = targetNodeId;
      site.targetAddress = target;
      site.owner = ownerFor(from);
      String mnemonic = instruction.getMnemonicString().toLowerCase(Locale.ROOT);
      if (mnemonic.equals("bctrl") || mnemonic.equals("bctr")) {
        site.kind = "unresolved_indirect";
      } else if (callableTarget && (instruction.getFlowType().isCall() ||
          reference.getReferenceType().isCall())) {
        site.kind = "direct_call";
        site.lr = address(from.add(4));
        site.arguments = traceArguments(site);
      } else if (callableTarget && (instruction.getFlowType().isJump() ||
          reference.getReferenceType().isJump())) {
        site.kind = "tail_call";
        site.arguments = traceArguments(site);
      } else {
        site.kind = callableTarget ? "non_call_reference" : "data_reference";
      }
      result.add(site);
    }
    sortSites(result);
    return result;
  }

  private void collectWrappers(ImportRecord record, Address targetAddress,
      String targetNodeId, int depth, Set<String> visited) throws Exception {
    if (depth > MAX_WRAPPER_DEPTH) {
      return;
    }
    List<Site> incoming = referencesTo(targetAddress, targetNodeId, true);
    for (Site site : incoming) {
      if (!(site.kind.equals("direct_call") || site.kind.equals("tail_call")) ||
          site.owner.function == null) {
        continue;
      }
      WrapperDecision decision = classifyWrapper(site.owner, targetAddress);
      if (!decision.qualified) {
        if (depth > 1) {
          record.wrapperFrontiers.add(site);
          addEdge(site);
        }
        continue;
      }
      String visitKey = record.id() + ":" + site.owner.id;
      if (!visited.add(visitKey)) {
        continue;
      }
      WrapperLink link = new WrapperLink();
      link.depth = depth;
      link.wrapper = site.owner;
      link.edgeToTarget = site;
      link.decision = decision;
      record.wrappers.add(link);
      addEdge(site);

      List<Site> wrapperIncoming = referencesTo(site.owner.entry, site.owner.id, true);
      boolean hadIncoming = false;
      for (Site caller : wrapperIncoming) {
        if (!(caller.kind.equals("direct_call") || caller.kind.equals("tail_call"))) {
          continue;
        }
        hadIncoming = true;
        if (caller.owner.function != null && depth < MAX_WRAPPER_DEPTH &&
            classifyWrapper(caller.owner, site.owner.entry).qualified) {
          collectWrappers(record, site.owner.entry, site.owner.id, depth + 1, visited);
        } else {
          record.wrapperFrontiers.add(caller);
          addEdge(caller);
        }
      }
      if (!hadIncoming) {
        // A bounded wrapper may be an address-taken entry with no direct caller.
      }
    }
    record.wrappers.sort(Comparator.comparingInt((WrapperLink link) -> link.depth)
        .thenComparing(link -> link.wrapper.id)
        .thenComparingLong(link -> link.edgeToTarget.address.getOffset() & 0xffffffffL));
    deduplicateSites(record.wrapperFrontiers);
  }

  private WrapperDecision classifyWrapper(OwnerNode owner, Address target) {
    WrapperDecision decision = new WrapperDecision();
    decision.bytes = (int) Math.min(Integer.MAX_VALUE, owner.size);
    if (owner.function == null) {
      decision.reason = "unowned_instruction_chunk";
      return decision;
    }
    if (owner.size > MAX_WRAPPER_BYTES) {
      decision.reason = "body_exceeds_0x100_bytes";
      return decision;
    }
    InstructionIterator iterator = currentProgram.getListing().getInstructions(
        owner.function.getBody(), true);
    while (iterator.hasNext()) {
      Instruction instruction = iterator.next();
      decision.instructions++;
      if (decision.instructions > MAX_WRAPPER_INSTRUCTIONS) {
        decision.reason = "body_exceeds_32_instructions";
        return decision;
      }
      String mnemonic = instruction.getMnemonicString().toLowerCase(Locale.ROOT);
      if (mnemonic.equals("bctrl") || mnemonic.equals("bctr")) {
        decision.unresolvedIndirectEdges++;
        continue;
      }
      Address[] flows = instruction.getFlows();
      if (flows == null) {
        continue;
      }
      for (Address flow : flows) {
        if (owner.function.getBody().contains(flow)) {
          continue;
        }
        if (flow.equals(target)) {
          decision.matchingTargetEdges++;
        } else if (instruction.getFlowType().isCall() ||
            instruction.getFlowType().isJump()) {
          decision.otherExternalEdges++;
        }
      }
    }
    if (decision.matchingTargetEdges != 1) {
      decision.reason = "requires_exactly_one_edge_to_target";
      return decision;
    }
    if (decision.otherExternalEdges != 0) {
      decision.reason = "has_other_external_control_flow";
      return decision;
    }
    if (decision.unresolvedIndirectEdges != 0) {
      decision.reason = "has_unresolved_bctr_or_bctrl";
      return decision;
    }
    decision.qualified = true;
    decision.kind = "bounded_sdk_wrapper";
    decision.reason = "single_external_target_small_bounded_body";
    return decision;
  }

  private OwnerNode ownerFor(Address address) throws Exception {
    FunctionManager manager = currentProgram.getFunctionManager();
    Function function = manager.getFunctionContaining(address);
    String id;
    if (function != null) {
      id = "function:" + ExportDemoSdkCallGraph.address(function.getEntryPoint());
    } else {
      id = "instruction_chunk:" + ExportDemoSdkCallGraph.address(address);
    }
    OwnerNode cached = nodes.get(id);
    if (cached != null) {
      return cached;
    }
    OwnerNode node = new OwnerNode();
    node.id = id;
    node.function = function;
    if (function != null) {
      node.kind = "function";
      node.name = function.getName();
      node.entry = function.getEntryPoint();
      AddressRangeIterator ranges = function.getBody().getAddressRanges(true);
      MessageDigest digest = MessageDigest.getInstance("SHA-256");
      while (ranges.hasNext()) {
        AddressRange range = ranges.next();
        long start = range.getMinAddress().getOffset() & 0xffffffffL;
        long end = range.getMaxAddress().getOffset() & 0xffffffffL;
        node.ranges.add(new long[] {start, end});
        node.size += end - start + 1;
        hashMemoryRange(digest, range.getMinAddress(), end - start + 1);
      }
      node.byteSha256 = hex(digest.digest());
    } else {
      node.kind = "bounded_instruction_chunk";
      node.name = "unowned_" + ExportDemoSdkCallGraph.address(address);
      node.entry = address;
      long start = address.getOffset() & 0xffffffffL;
      node.ranges.add(new long[] {start, start + 3});
      node.size = 4;
      node.byteSha256 = hashRange(address, 4);
    }
    nodes.put(node.id, node);
    return node;
  }

  private List<Map<String, Object>> traceArguments(Site site) {
    List<Map<String, Object>> result = new ArrayList<>();
    for (int register = 3; register <= 10; register++) {
      result.add(traceRegister("r" + register, site.address, site.owner));
    }
    return result;
  }

  private Map<String, Object> traceRegister(String initialRegister, Address before,
      OwnerNode owner) {
    Map<String, Object> result = map();
    result.put("argument_register", initialRegister);
    result.put("bounds", mapOf(
        "max_chain_steps", MAX_PROVENANCE_CHAIN,
        "max_instructions_per_step", MAX_PROVENANCE_SCAN,
        "scope", "owner_function_and_control-flow-linear"));
    List<Map<String, Object>> steps = new ArrayList<>();
    result.put("steps", steps);
    if (owner.function == null) {
      result.put("status", "unresolved_unowned_chunk");
      return result;
    }
    String currentRegister = initialRegister;
    Address cursor = before;
    Set<String> seen = new HashSet<>();
    for (int chain = 0; chain < MAX_PROVENANCE_CHAIN; chain++) {
      String stateKey = currentRegister + "@" + address(cursor);
      if (!seen.add(stateKey)) {
        result.put("status", "unresolved_cycle");
        result.put("origin_register", currentRegister);
        return result;
      }
      Instruction producer = null;
      int scanned = 0;
      Instruction previous = currentProgram.getListing().getInstructionBefore(cursor);
      while (previous != null && owner.function.getBody().contains(previous.getAddress()) &&
          scanned < MAX_PROVENANCE_SCAN) {
        scanned++;
        if (writesRegister(previous, currentRegister)) {
          producer = previous;
          break;
        }
        String mnemonic = previous.getMnemonicString().toLowerCase(Locale.ROOT);
        if (previous.getFlowType().isCall() && volatileAcrossCall(currentRegister)) {
          result.put("status", "unresolved_prior_call_clobber");
          result.put("origin_register", currentRegister);
          result.put("boundary_address", address(previous.getAddress()));
          return result;
        }
        if ((previous.getFlowType().isJump() || previous.getFlowType().isConditional()) &&
            !mnemonic.equals("blr") && !mnemonic.equals("bclr")) {
          result.put("status", "unresolved_control_flow_merge");
          result.put("origin_register", currentRegister);
          result.put("boundary_address", address(previous.getAddress()));
          return result;
        }
        previous = currentProgram.getListing().getInstructionBefore(previous.getAddress());
      }
      if (producer == null) {
        result.put("origin_register", currentRegister);
        result.put("status", scanned >= MAX_PROVENANCE_SCAN ?
            "unresolved_scan_bound" : "function_entry_register");
        return result;
      }
      List<String> inputs = gprInputs(producer);
      Map<String, Object> step = map();
      step.put("address", address(producer.getAddress()));
      step.put("instruction", producer.toString());
      step.put("instruction_sha256", hashRangeUnchecked(producer.getAddress(), 4));
      step.put("writes", currentRegister);
      step.put("input_gprs", inputs);
      steps.add(step);
      String mnemonic = producer.getMnemonicString().toLowerCase(Locale.ROOT);
      if (isConstantProducer(mnemonic, inputs)) {
        result.put("status", "constant_or_immediate");
        result.put("origin_instruction", producer.toString());
        return result;
      }
      if (isMemoryLoad(mnemonic)) {
        result.put("status", "memory_load");
        result.put("origin_instruction", producer.toString());
        return result;
      }
      Set<String> uniqueInputs = new TreeSet<>(inputs);
      uniqueInputs.remove(currentRegister);
      if (isRegisterTransform(mnemonic) && uniqueInputs.size() == 1) {
        currentRegister = uniqueInputs.iterator().next();
        cursor = producer.getAddress();
        continue;
      }
      result.put("status", "instruction_defined");
      result.put("origin_instruction", producer.toString());
      return result;
    }
    result.put("status", "unresolved_chain_bound");
    result.put("origin_register", currentRegister);
    return result;
  }

  private boolean writesRegister(Instruction instruction, String register) {
    for (Object object : instruction.getResultObjects()) {
      if (object != null && object.toString().equalsIgnoreCase(register)) {
        return true;
      }
    }
    return false;
  }

  private List<String> gprInputs(Instruction instruction) {
    Set<String> result = new TreeSet<>(Comparator.comparingInt(value ->
        Integer.parseInt(value.substring(1))));
    for (Object object : instruction.getInputObjects()) {
      if (object == null) {
        continue;
      }
      String value = object.toString().toLowerCase(Locale.ROOT);
      if (value.matches("r([0-9]|[12][0-9]|3[01])")) {
        result.add(value);
      }
    }
    return new ArrayList<>(result);
  }

  private boolean isConstantProducer(String mnemonic, List<String> inputs) {
    return mnemonic.equals("li") || mnemonic.equals("lis");
  }

  private boolean volatileAcrossCall(String register) {
    if (!register.matches("r([0-9]|[12][0-9]|3[01])")) {
      return true;
    }
    int number = Integer.parseInt(register.substring(1));
    return number == 0 || (number >= 3 && number <= 12);
  }

  private boolean isMemoryLoad(String mnemonic) {
    return mnemonic.startsWith("lb") || mnemonic.startsWith("lh") ||
        mnemonic.startsWith("lw") || mnemonic.startsWith("ld") ||
        mnemonic.startsWith("lf") || mnemonic.startsWith("lv") ||
        mnemonic.startsWith("lve") || mnemonic.startsWith("dcb");
  }

  private boolean isRegisterTransform(String mnemonic) {
    return mnemonic.equals("mr") || mnemonic.equals("or") || mnemonic.equals("ori") ||
        mnemonic.equals("oris") || mnemonic.equals("xor") || mnemonic.equals("xori") ||
        mnemonic.equals("xoris") || mnemonic.equals("addi") || mnemonic.equals("addis") ||
        mnemonic.equals("addic") || mnemonic.equals("subf") || mnemonic.equals("neg") ||
        mnemonic.startsWith("rlw") || mnemonic.startsWith("rld") ||
        mnemonic.startsWith("exts") || mnemonic.startsWith("clr") ||
        mnemonic.startsWith("rotl") || mnemonic.startsWith("sl") ||
        mnemonic.startsWith("sr");
  }

  private void addEdge(Site site) {
    edges.put(site.edgeId(), siteMap(site, true));
  }

  private void collectUnresolvedIndirects() {
    for (OwnerNode node : nodes.values()) {
      if (node.function == null) {
        continue;
      }
      InstructionIterator iterator = currentProgram.getListing().getInstructions(
          node.function.getBody(), true);
      while (iterator.hasNext()) {
        Instruction instruction = iterator.next();
        String mnemonic = instruction.getMnemonicString().toLowerCase(Locale.ROOT);
        if (!mnemonic.equals("bctrl") && !mnemonic.equals("bctr")) {
          continue;
        }
        Map<String, Object> item = map();
        item.put("address", address(instruction.getAddress()));
        item.put("instruction", instruction.toString());
        item.put("instruction_sha256", hashRangeUnchecked(instruction.getAddress(), 4));
        item.put("owner_node_id", node.id);
        item.put("resolution", "unresolved");
        item.put("reason", "no_qualified_vtable_and_slot_evidence_in_this_export");
        unresolvedIndirect.put(address(instruction.getAddress()), item);
      }
    }
  }

  private Map<String, Object> buildDocument(List<ImportRecord> imports) throws Exception {
    Map<String, Object> document = map();
    document.put("schema", SCHEMA);
    document.put("target_id", TARGET_ID);
    Map<String, Object> identity = map();
    identity.put("project_path", PROJECT_PATH);
    identity.put("project", PROJECT);
    identity.put("project_locator", projectLocator());
    identity.put("program", PROGRAM);
    identity.put("module", PROGRAM);
    identity.put("xex_sha256", XEX_SHA256);
    identity.put("language", LANGUAGE);
    identity.put("compiler_spec", COMPILER_SPEC);
    identity.put("image_base", address(IMAGE_BASE));
    identity.put("ghidra_database_image_base", address(currentProgram.getImageBase()));
    identity.put("qualified_ranges", mapOf(
        ".text", mapOf("address", address(TEXT_START), "size", address(TEXT_SIZE),
            "byte_sha256", TEXT_SHA256),
        ".pdata", mapOf("address", address(PDATA_START), "size", address(PDATA_SIZE),
            "byte_sha256", PDATA_SHA256)));
    document.put("identity", identity);
    document.put("ghidra", mapOf(
        "version", "12.1.2",
        "loader", "XEX Loader by Warranty Voider",
        "language", LANGUAGE,
        "compiler_spec", COMPILER_SPEC));
    document.put("invocation_contract", mapOf(
        "ghidra_access", "read-only",
        "analysis", "disabled",
        "required_headless_flags", Arrays.asList("-readOnly", "-noanalysis"),
        "script_mode_argument", "read-only-noanalysis"));
    document.put("bounds", mapOf(
        "wrapper_max_depth", MAX_WRAPPER_DEPTH,
        "wrapper_max_bytes", address(MAX_WRAPPER_BYTES),
        "wrapper_max_instructions", MAX_WRAPPER_INSTRUCTIONS,
        "argument_registers", Arrays.asList("r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10"),
        "argument_scan_max_instructions_per_step", MAX_PROVENANCE_SCAN,
        "argument_chain_max_steps", MAX_PROVENANCE_CHAIN));
    document.put("indirect_call_policy", mapOf(
        "bctr_bctrl", "unresolved_without_qualified_vtable_and_slot",
        "qualified_indirect_edges", Collections.emptyList(),
        "unresolved_count", unresolvedIndirect.size()));
    document.put("relations", mapOf(
        "import_direct_calls", "imports[].direct_edge_ids -> edges[].id",
        "import_non_call_references", "imports[].non_call_edge_ids -> edges[].id",
        "wrapper_edges", "imports[].bounded_wrappers[].edge_to_target_id -> edges[].id",
        "wrapper_frontiers", "imports[].wrapper_frontier_edge_ids -> edges[].id",
        "edge_owners", "edges[].source_node_id -> owner_nodes[].id"));

    List<Map<String, Object>> importMaps = new ArrayList<>();
    for (ImportRecord record : imports) {
      importMaps.add(importMap(record));
    }
    document.put("imports", importMaps);
    List<Map<String, Object>> nodeMaps = new ArrayList<>();
    for (OwnerNode node : nodes.values()) {
      nodeMaps.add(nodeMap(node));
    }
    document.put("owner_nodes", nodeMaps);
    document.put("edges", new ArrayList<>(edges.values()));
    document.put("unresolved_indirect_callsites",
        new ArrayList<>(unresolvedIndirect.values()));
    document.put("counts", mapOf(
        "imports", imports.size(),
        "callable_imports", countKind(imports, "callable"),
        "variable_imports", countKind(imports, "variable"),
        "xam_imports", EXPECTED_XAM_IMPORTS,
        "xboxkrnl_imports", EXPECTED_XBOXKRNL_IMPORTS,
        "owner_nodes", nodes.size(),
        "edges", edges.size(),
        "unresolved_indirect_callsites", unresolvedIndirect.size()));
    document.put("limitations", Arrays.asList(
        "Static direct references only; runtime-created function pointers are not inferred.",
        "The ten kVariable imports have slot references, not callable stubs.",
        "Argument provenance is control-flow-linear, register-local and explicitly bounded.",
        "No bctr/bctrl target is attributed without separate qualified vtable/slot evidence."));
    return document;
  }

  private String projectLocator() {
    return PROJECT_PATH;
  }

  private Map<String, Object> importMap(ImportRecord record) {
    Map<String, Object> item = map();
    item.put("id", record.id());
    item.put("module", record.module);
    item.put("name", record.name);
    if (!record.ghidraName.equals(record.name)) {
      item.put("ghidra_symbol_name", record.ghidraName);
    }
    item.put("ordinal", record.ordinal);
    item.put("kind", record.kind);
    item.put("slot_address", address(record.slot));
    item.put("slot_record", rawRecord(record.slot, record.slotRaw));
    item.put("stub_address", record.stub == null ? null : address(record.stub));
    item.put("stub_record", record.stub == null ? null : rawRecord(record.stub,
        record.stubRaw.intValue()));
    List<String> callers = new ArrayList<>();
    List<String> edgeIds = new ArrayList<>();
    for (Site site : record.direct) {
      callers.add(site.owner.id);
      edgeIds.add(site.edgeId());
    }
    item.put("direct_caller_node_ids", sortedUnique(callers));
    item.put("direct_edge_ids", sortedUnique(edgeIds));
    List<String> nonCallEdgeIds = new ArrayList<>();
    for (Site site : record.nonCall) {
      nonCallEdgeIds.add(site.edgeId());
    }
    item.put("non_call_edge_ids", sortedUnique(nonCallEdgeIds));
    item.put("rejected_indirect_references", siteMaps(record.rejectedIndirect, false));
    List<Map<String, Object>> wrappers = new ArrayList<>();
    for (WrapperLink link : record.wrappers) {
      Map<String, Object> wrapper = map();
      wrapper.put("depth", link.depth);
      wrapper.put("wrapper_node_id", link.wrapper.id);
      wrapper.put("edge_to_target_id", link.edgeToTarget.edgeId());
      wrapper.put("qualification", wrapperDecisionMap(link.decision));
      wrappers.add(wrapper);
    }
    item.put("bounded_wrappers", wrappers);
    List<String> frontierEdgeIds = new ArrayList<>();
    for (Site site : record.wrapperFrontiers) {
      frontierEdgeIds.add(site.edgeId());
    }
    item.put("wrapper_frontier_edge_ids", sortedUnique(frontierEdgeIds));
    item.put("counts", mapOf(
        "direct_callsites", record.direct.size(),
        "direct_callers", sortedUnique(callers).size(),
        "non_call_references", record.nonCall.size(),
        "bounded_wrappers", record.wrappers.size(),
        "wrapper_frontier_callsites", record.wrapperFrontiers.size(),
        "rejected_indirect_references", record.rejectedIndirect.size()));
    return item;
  }

  private Map<String, Object> rawRecord(Address address, int raw) {
    Map<String, Object> item = map();
    item.put("address", ExportDemoSdkCallGraph.address(address));
    item.put("raw_be_u32", String.format(Locale.ROOT, "0x%08X", raw));
    item.put("byte_sha256", hashRangeUnchecked(address, 4));
    return item;
  }

  private Map<String, Object> wrapperDecisionMap(WrapperDecision decision) {
    return mapOf(
        "qualified", decision.qualified,
        "kind", decision.kind,
        "reason", decision.reason,
        "body_bytes", address(decision.bytes),
        "instruction_count", decision.instructions,
        "matching_target_edges", decision.matchingTargetEdges,
        "other_external_edges", decision.otherExternalEdges,
        "unresolved_indirect_edges", decision.unresolvedIndirectEdges);
  }

  private Map<String, Object> nodeMap(OwnerNode node) {
    Map<String, Object> item = map();
    item.put("id", node.id);
    item.put("kind", node.kind);
    item.put("name", node.name);
    item.put("entry", address(node.entry));
    item.put("size", address(node.size));
    item.put("byte_sha256", node.byteSha256);
    List<List<String>> ranges = new ArrayList<>();
    for (long[] range : node.ranges) {
      ranges.add(Arrays.asList(address(range[0]), address(range[1])));
    }
    item.put("ranges", ranges);
    return item;
  }

  private List<Map<String, Object>> siteMaps(List<Site> sites, boolean includeId) {
    List<Map<String, Object>> result = new ArrayList<>();
    for (Site site : sites) {
      result.add(siteMap(site, includeId));
    }
    return result;
  }

  private Map<String, Object> siteMap(Site site, boolean includeId) {
    Map<String, Object> item = map();
    if (includeId) {
      item.put("id", site.edgeId());
    }
    item.put("kind", site.kind);
    item.put("source_node_id", site.owner.id);
    item.put("target_node_id", site.targetNodeId);
    item.put("target_address", address(site.targetAddress));
    item.put("callsite", address(site.address));
    item.put("instruction", site.instruction);
    item.put("instruction_sha256", site.instructionSha256);
    item.put("reference_type", site.referenceType);
    item.put("lr", site.lr);
    item.put("arguments", site.arguments);
    return item;
  }

  private void sortSites(List<Site> sites) {
    sites.sort(Comparator.comparingLong((Site site) ->
        site.address.getOffset() & 0xffffffffL).thenComparing(site -> site.targetNodeId)
        .thenComparing(site -> site.kind));
  }

  private void deduplicateSites(List<Site> sites) {
    Map<String, Site> unique = new TreeMap<>();
    for (Site site : sites) {
      unique.put(site.edgeId(), site);
    }
    sites.clear();
    sites.addAll(unique.values());
    sortSites(sites);
  }

  private List<String> sortedUnique(List<String> values) {
    return new ArrayList<>(new TreeSet<>(values));
  }

  private String hashRange(Address start, long size) throws Exception {
    MessageDigest digest = MessageDigest.getInstance("SHA-256");
    hashMemoryRange(digest, start, size);
    return hex(digest.digest());
  }

  private String hashRangeUnchecked(Address start, long size) {
    try {
      return hashRange(start, size);
    } catch (Exception error) {
      return null;
    }
  }

  private void hashMemoryRange(MessageDigest digest, Address start, long size)
      throws Exception {
    byte[] buffer = new byte[4096];
    long offset = 0;
    while (offset < size) {
      int count = (int) Math.min(buffer.length, size - offset);
      int read = currentProgram.getMemory().getBytes(start.add(offset), buffer, 0, count);
      if (read != count) {
        throw new IllegalStateException("short memory read at " + start.add(offset));
      }
      digest.update(buffer, 0, count);
      offset += count;
    }
  }

  private static String sha256(byte[] bytes) throws Exception {
    return hex(MessageDigest.getInstance("SHA-256").digest(bytes));
  }

  private static String hex(byte[] bytes) {
    StringBuilder builder = new StringBuilder(bytes.length * 2);
    for (byte value : bytes) {
      builder.append(String.format(Locale.ROOT, "%02x", value & 0xff));
    }
    return builder.toString();
  }

  private static String address(Address address) {
    return address(address.getOffset() & 0xffffffffL);
  }

  private static String address(long value) {
    return String.format(Locale.ROOT, "0x%08X", value & 0xffffffffL);
  }

  private static Map<String, Object> map() {
    return new LinkedHashMap<>();
  }

  private static Map<String, Object> mapOf(Object... values) {
    Map<String, Object> result = map();
    for (int index = 0; index < values.length; index += 2) {
      result.put((String) values[index], values[index + 1]);
    }
    return result;
  }

  @SuppressWarnings("unchecked")
  private static String json(Object value, int indent) {
    if (value == null) {
      return "null";
    }
    if (value instanceof String) {
      return quote((String) value);
    }
    if (value instanceof Number || value instanceof Boolean) {
      return value.toString();
    }
    String padding = " ".repeat(indent);
    String childPadding = " ".repeat(indent + 2);
    if (value instanceof Map) {
      Map<String, Object> sorted = new TreeMap<>((Map<String, Object>) value);
      if (sorted.isEmpty()) {
        return "{}";
      }
      StringBuilder builder = new StringBuilder("{\n");
      int index = 0;
      for (Map.Entry<String, Object> entry : sorted.entrySet()) {
        builder.append(childPadding).append(quote(entry.getKey())).append(": ")
            .append(json(entry.getValue(), indent + 2));
        if (++index != sorted.size()) {
          builder.append(',');
        }
        builder.append('\n');
      }
      return builder.append(padding).append('}').toString();
    }
    if (value instanceof Iterable) {
      List<Object> items = new ArrayList<>();
      for (Object item : (Iterable<Object>) value) {
        items.add(item);
      }
      if (items.isEmpty()) {
        return "[]";
      }
      StringBuilder builder = new StringBuilder("[\n");
      for (int index = 0; index < items.size(); index++) {
        builder.append(childPadding).append(json(items.get(index), indent + 2));
        if (index + 1 != items.size()) {
          builder.append(',');
        }
        builder.append('\n');
      }
      return builder.append(padding).append(']').toString();
    }
    throw new IllegalArgumentException("unsupported JSON value: " + value.getClass());
  }

  private static String quote(String value) {
    StringBuilder builder = new StringBuilder("\"");
    for (int index = 0; index < value.length(); index++) {
      char character = value.charAt(index);
      switch (character) {
        case '\\': builder.append("\\\\"); break;
        case '"': builder.append("\\\""); break;
        case '\n': builder.append("\\n"); break;
        case '\r': builder.append("\\r"); break;
        case '\t': builder.append("\\t"); break;
        default:
          if (character < 0x20) {
            builder.append(String.format(Locale.ROOT, "\\u%04x", (int) character));
          } else {
            builder.append(character);
          }
      }
    }
    return builder.append('"').toString();
  }
}
