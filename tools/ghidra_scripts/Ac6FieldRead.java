// Field reads of a given struct offset, with vtable dispatch excluded.
//
// A grep for "0xf0(r" cannot answer "who reads this field", because on this
// image most such instructions are virtual calls: the compiler emits
//
//     lwz r11,0x0(r3)      ; r11 = the object's vtable
//     lwz r11,0xf0(r11)    ; r11 = the method at slot +0xF0
//     mtspr CTR,r11
//     bctrl
//
// where +0xF0 is a slot index and not a field at all. Cycle 1145 read such a
// sequence as a route-cursor read and had to correct it.
//
// The discriminator used here is the one the codegen guarantees: a vtable slot
// load is *consumed by mtspr CTR* within the next few instructions, and a field
// read is not. The script also reports whether the base register was itself
// loaded from displacement 0 of another register just before, which is the
// other half of the same signature, so a reader can see both signals rather
// than trusting one.
//
// usage: Ac6FieldRead OUT 0xf0 [more offsets...]
import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

public class Ac6FieldRead extends GhidraScript {

  // Does an mtspr CTR consume `reg` within `window` instructions?
  private boolean feedsCounter(Instruction start, String reg, int window) {
    Instruction ins = start;
    for (int i = 0; i < window && ins != null; ++i) {
      ins = ins.getNext();
      if (ins == null) break;
      String t = ins.toString();
      if (t.startsWith("mtspr CTR," + reg)) return true;
      // The register was overwritten by something else first.
      if (t.matches("^(lwz|lis|li|addi|or)\\s+" + reg + ",.*")) return false;
    }
    return false;
  }

  @Override
  public void run() throws Exception {
    String[] args = getScriptArgs();
    PrintWriter out = new PrintWriter(args[0], "UTF-8");
    long total = 0, field = 0, dispatch = 0, stack = 0;
    InstructionIterator it = currentProgram.getListing().getInstructions(true);
    while (it.hasNext()) {
      Instruction ins = it.next();
      total++;
      String text = ins.toString();
      if (!text.startsWith("lwz ")) continue;
      for (int i = 1; i < args.length; ++i) {
        String off = args[i].toLowerCase();
        // lwz rDST,OFF(rBASE)
        String body = text.substring(4).trim();
        int comma = body.indexOf(',');
        if (comma < 0) continue;
        String dst = body.substring(0, comma).trim();
        String rest = body.substring(comma + 1).trim();
        int paren = rest.indexOf('(');
        if (paren < 0 || !rest.endsWith(")")) continue;
        String disp = rest.substring(0, paren).trim().toLowerCase();
        String base = rest.substring(paren + 1, rest.length() - 1).trim();
        if (!disp.equals(off)) continue;
        // r1 is the stack pointer: `lwz rX,0xf0(r1)` is a spill slot, not a
        // struct field, and counting it inflates the answer by a third.
        if (base.equals("r1")) { stack++; continue; }

        boolean isDispatch = feedsCounter(ins, dst, 4);
        // Was the base itself loaded from displacement 0 just before?
        boolean baseFromZero = false;
        Instruction prev = ins.getPrevious();
        for (int k = 0; k < 3 && prev != null; ++k, prev = prev.getPrevious()) {
          String p = prev.toString();
          if (p.matches("^lwz\\s+" + base + ",0x0\\(r\\d+\\)$")) { baseFromZero = true; break; }
        }
        Function f = getFunctionContaining(ins.getAddress());
        if (isDispatch) {
          dispatch++;
        } else {
          field++;
          out.println("FIELD " + off + "  at " + ins.getAddress() + "  in " +
                      (f == null ? "none" : f.getName() + "@" + f.getEntryPoint()) +
                      "  " + text + (baseFromZero ? "   [base loaded from +0x0 - suspect]" : ""));
        }
      }
    }
    out.println("scanned " + total + " field_reads " + field + " vtable_dispatch " + dispatch +
                " stack_slots " + stack);
    out.close();
    println("WROTE " + args[0] + " field=" + field + " dispatch=" + dispatch + " stack=" + stack);
  }
}
