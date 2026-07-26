# gdb -x this script. Runs ac6recomp, interrupts after the guest has gone
# quiet, and dumps every thread's stack. ptrace_scope=1 forbids attaching to a
# running process, so the process must be launched under gdb.
import gdb
import threading
import time

# ReXGlue installs a SIGSEGV handler to implement guest MMIO and page
# protection, so SIGSEGV is normal operation, not a fault. Without passing it
# through, gdb stops on the first guest memory access and the resulting stack
# is a memory-trap site, not a stall -- which is exactly how cycle 308
# misattributed the boot stall. See cycle-309.

DELAY = float(gdb.parameter("max-value-size") and 0 or 0) or 45.0


def interrupt_later():
    time.sleep(DELAY)
    gdb.post_event(lambda: gdb.execute("interrupt"))


def on_stop(event):
    # Only dump once, on the manual interrupt.
    if getattr(on_stop, "done", False):
        return
    on_stop.done = True
    try:
        gdb.execute("info threads")
        gdb.execute("thread apply all bt 18")
    except gdb.error as exc:
        print("dump failed:", exc)
    gdb.execute("kill")
    gdb.execute("quit")


for _sig in ("SIGSEGV", "SIGBUS", "SIGUSR1", "SIGUSR2"):
    try:
        gdb.execute("handle %s nostop noprint pass" % _sig, to_string=True)
    except gdb.error:
        pass

gdb.events.stop.connect(on_stop)
threading.Thread(target=interrupt_later, daemon=True).start()
gdb.execute("run")
