"""Inspect candidate parser function addresses in the canonical Ghidra project."""

# Ghidra injects these script globals at runtime.
# ruff: noqa: F821

from ghidra.util.task import ConsoleTaskMonitor

def addr(program, value):
    return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(value)

parser_candidates = [
    (0x82309620, "SubMisTblBin::getReadBuffSize"),
    (0x82309758, "SubMisTblBin::read"),
    (0x8232C7E0, "SubMisBin::getReadBuffSize"),
    (0x8232C8A8, "SubMisBin::read"),
    (0x8232F4D0, "SetBin::getReadBuffSize"),
    (0x8232F5F8, "SetBin::read"),
    (0x8232FF78, "ObjBin::getReadBuffSize"),
    (0x82330158, "ObjBin::read"),
    (0x82330540, "ActBin::getReadBuffSize"),
    (0x82330688, "ActBin::read"),
    (0x823310E8, "OrderBin::getReadBuffSize"),
    (0x82331208, "OrderBin::read"),
    (0x823316A0, "ManeuverBin::getReadBuffSize"),
    (0x82331808, "ManeuverBin::read"),
    (0x82331BB0, "ComTblBin::getReadBuffSize"),
    (0x82331C10, "ComTblBin::read"),
    (0x82331D98, "ComBin::read"),
]

monitor = ConsoleTaskMonitor()
print(f"IMAGE_BASE {currentProgram.getImageBase()}")

for pc_addr, label in parser_candidates:
    a = addr(currentProgram, pc_addr)
    func_mgr = currentProgram.getFunctionManager()
    fn = func_mgr.getFunctionAt(a)
    
    if fn is None:
        print(f"0x{pc_addr:08X} {label}: NO_FUNCTION")
        continue
    
    # Get callers
    refs = getReferencesTo(a)
    callers = set()
    for ref in refs:
        caller_fn = getFunctionContaining(ref.getFromAddress())
        cname = caller_fn.getName() if caller_fn else "data"
        callers.add(f"{cname}@{ref.getFromAddress()}")
    
    # Get callees (bl/bctrl within this function)
    listing = currentProgram.getListing()
    callees = set()
    for ins in listing.getInstructions(fn.getBody(), True):
        if ins.getMnemonicString() in ("bl", "bctrl"):
            for r in ins.getReferencesFrom():
                target_fn = func_mgr.getFunctionAt(r.getToAddress())
                if target_fn:
                    callees.add(f"{target_fn.getName()}@{r.getToAddress()}")
    
    body_size = fn.getBody().getNumAddresses()
    print(f"0x{pc_addr:08X} {label}: name={fn.getName()} body={body_size} callers={len(callers)} callees={len(callees)}")
    
    ci = 0
    for c in sorted(callers):
        if ci >= 12:
            break
        print(f"  CALLER {c}")
        ci += 1
    
    cc = 0
    for c in sorted(callees):
        if cc >= 12:
            break
        print(f"  CALLEE {c}")
        cc += 1
