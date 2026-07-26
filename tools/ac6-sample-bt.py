# gdb -x this script. Interrupts the guest at several instants and dumps every
# thread, so a stalled boot can be told apart from a slow one: if the frames
# change between samples, the guest is progressing.
#
# ptrace_scope=1 forbids attaching, so the process must be launched under gdb.
#
# ReXGlue installs a SIGSEGV handler to implement guest MMIO and page
# protection, so SIGSEGV is normal operation, not a fault. gdb must pass those
# through untouched or it stops on every guest memory access.
import gdb
import os
import threading
import time

SAMPLES = [int(s) for s in os.environ.get("AC6_SAMPLE_AT", "10,30,60").split(",")]
state = {"i": 0, "t0": time.time(), "want": False}

for sig in ("SIGSEGV", "SIGBUS", "SIGUSR1", "SIGUSR2", "SIGPIPE", "SIG34", "SIG35"):
    try:
        gdb.execute(f"handle {sig} nostop noprint pass", to_string=True)
    except gdb.error:
        pass
gdb.execute("set confirm off")
gdb.execute("set pagination off")


def driver():
    prev = 0
    for t in SAMPLES:
        time.sleep(max(0, t - prev))
        prev = t
        state["want"] = True
        gdb.post_event(lambda: gdb.execute("interrupt"))
        time.sleep(6)
    gdb.post_event(lambda: gdb.execute("kill"))
    time.sleep(1)
    gdb.post_event(lambda: gdb.execute("quit"))


def on_stop(event):
    if not state["want"]:
        gdb.post_event(lambda: gdb.execute("continue"))
        return
    state["want"] = False
    i = state["i"]
    state["i"] = i + 1
    print(f"\n===== SAMPLE {i} at t={time.time() - state['t0']:.0f}s =====")
    try:
        gdb.execute("thread apply all bt 12")
    except gdb.error as exc:
        print("dump failed:", exc)
    gdb.post_event(lambda: gdb.execute("continue"))


gdb.events.stop.connect(on_stop)
gdb.execute("run")
