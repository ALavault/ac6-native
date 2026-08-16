set pagination off
set confirm off
set debuginfod enabled off

python
import gdb

class QueueConsumerRaw(gdb.Breakpoint):
    """Read-only raw context probe; never requires an active bridge/TLS."""

    def __init__(self):
        self.hits = 0
        super().__init__('__imp__sub_820FEFA8', gdb.BP_BREAKPOINT,
                         internal=False)
        self.silent = True

    def stop(self):
        try:
            ctx = int(gdb.parse_and_eval('$rdi'))

            def gpr(index):
                offsets = {0: 8, 1: 16, 2: 24, 3: 0}
                if index not in offsets:
                    offsets[index] = 32 + (index - 4) * 8
                return int(gdb.parse_and_eval(
                    '*(unsigned long*)(%d+%d)' %
                    (ctx, offsets[index]))) & 0xFFFFFFFF

            lr = int(gdb.parse_and_eval(
                '*(unsigned long*)(%d+0x100)' % ctx)) & 0xFFFFFFFF
            pc = int(gdb.parse_and_eval('$pc')) & 0xFFFFFFFFFFFFFFFF
            tick = 'unknown'
            thread = 'unknown'
            try:
                bridge = int(gdb.parse_and_eval(
                    "'(anonymous namespace)::active_bridge'"))
                if bridge:
                    tick = str(int(gdb.parse_and_eval(
                        '*(unsigned long*)(%d+0x148)' % bridge)))
                thread = str(int(gdb.parse_and_eval(
                    "'(anonymous namespace)::current_guest_thread_id'")))
            except gdb.error:
                pass

            print('AC6_QUEUE_CONSUMER_RAW hit=%d tick=%s thread=%s '
                  'host_pc=0x%016x lr=0x%08x r3=0x%08x r4=0x%08x '
                  'r5=0x%08x r6=0x%08x r7=0x%08x r31=0x%08x' %
                  (self.hits, tick, thread, pc, lr, gpr(3), gpr(4),
                   gpr(5), gpr(6), gpr(7), gpr(31)))
            self.hits += 1
            if self.hits >= 32:
                self.enabled = False
        except gdb.error as exc:
            print('AC6_QUEUE_CONSUMER_RAW_ERROR %s' % exc)
        return False

QueueConsumerRaw()
end
continue
quit
