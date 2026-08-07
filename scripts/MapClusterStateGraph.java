// Emit the reference graph of a function range, tagged by reference kind.
//
// In the hierarchical state machine of cycle 1109, a state is never called: it
// is *named*. A handler names its superstate by materialising that handler's
// address into a member pointer, and names a transition target the same way,
// then hands it to the transition engine. So the states of a cluster are the
// functions that other functions reference as data, and the transitions are
// the data references that accompany a call to the engine.
//
// This script does not decide which is which - it reports every edge with its
// kind, plus how many times each function calls the engine, and leaves the
// classification to whoever reads it. Read-only.
//
// usage: MapClusterStateGraph.java LOW HIGH ENGINE
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.RefType;

import java.util.ArrayList;
import java.util.List;
import java.util.TreeMap;
import java.util.TreeSet;

public class MapClusterStateGraph extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 3) {
            printerr("usage: MapClusterStateGraph.java LOW HIGH ENGINE");
            return;
        }
        long low = Long.decode(args[0]);
        long high = Long.decode(args[1]);
        long engine = Long.decode(args[2]);

        List<Function> functions = new ArrayList<>();
        for (Function function : currentProgram.getFunctionManager().getFunctions(true)) {
            long entry = function.getEntryPoint().getOffset();
            if (entry >= low && entry < high) {
                functions.add(function);
            }
        }
        println("AC6_GRAPH_FUNCTIONS " + functions.size());

        for (Function function : functions) {
            if (monitor.isCancelled()) {
                return;
            }
            TreeMap<String, String> dataEdges = new TreeMap<>();  // target -> last site
            TreeSet<String> callEdges = new TreeSet<>();
            int engineCalls = 0;
            int signalTests = 0;

            for (Instruction instruction : currentProgram.getListing()
                    .getInstructions(function.getBody(), true)) {
                // A handler recognises its signals by comparing the third
                // argument against small negative constants.
                if (instruction.getMnemonicString().startsWith("cmp")) {
                    String text = instruction.toString();
                    if (text.contains("-0x1") || text.contains("-0x2")
                            || text.contains("-0x3") || text.contains("-0x4")
                            || text.contains("-0x5")) {
                        signalTests++;
                    }
                }
                for (Reference reference : instruction.getReferencesFrom()) {
                    Address target = reference.getToAddress();
                    long value = target.getOffset();
                    if (value == engine) {
                        engineCalls++;
                        continue;
                    }
                    if (value < low || value >= high) {
                        continue;
                    }
                    Function referenced =
                            currentProgram.getFunctionManager().getFunctionAt(target);
                    if (referenced == null || referenced.equals(function)) {
                        continue;
                    }
                    RefType type = reference.getReferenceType();
                    if (type.isCall()) {
                        callEdges.add(target.toString());
                    } else {
                        // Keep the latest site: a handler returns its
                        // superstate on the fall-through path, at the end.
                        dataEdges.put(target.toString(), instruction.getAddress().toString());
                    }
                }
            }
            List<String> rendered = new ArrayList<>();
            for (var pair : dataEdges.entrySet()) {
                rendered.add(pair.getKey() + "@" + pair.getValue());
            }
            println(String.format(
                    "AC6_NODE %s engine_calls=%d signal_tests=%d data=[%s] call=[%s]",
                    function.getEntryPoint(), engineCalls, signalTests,
                    String.join(",", rendered), String.join(",", callEdges)));
        }
    }
}
