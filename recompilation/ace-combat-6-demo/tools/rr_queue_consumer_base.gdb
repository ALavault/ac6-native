set pagination off
set confirm off
set debuginfod enabled off

python
import gdb

class QueueConsumerBase(gdb.Breakpoint):
    def __init__(self):
        self.hits = 0
        super().__init__('__imp__sub_820FEFA8', gdb.BP_BREAKPOINT,
                         internal=False)
        self.silent = True

    def stop(self):
        try:
            ctx = int(gdb.parse_and_eval('$rdi'))
            base = int(gdb.parse_and_eval('$rsi'))
            def gpr(index):
                offsets = {0: 8, 1: 16, 2: 24, 3: 0}
                offset = offsets.get(index, 32 + (index - 4) * 8)
                return int(gdb.parse_and_eval(
                    '*(unsigned long*)(%d+%d)' % (ctx, offset))) & 0xFFFFFFFF
            print('AC6_QUEUE_CONSUMER_BASE hit=%d ctx=0x%016x base=0x%016x '
                  'r3=0x%08x r31=0x%08x rsi=0x%016x fs=0x%016x' %
                  (self.hits, ctx, base, gpr(3), gpr(31), base,
                   int(gdb.parse_and_eval('$fs_base'))))
            self.hits += 1
            if self.hits >= 1:
                self.enabled = False
        except gdb.error as exc:
            print('AC6_QUEUE_CONSUMER_BASE_ERROR %s' % exc)
        return False

QueueConsumerBase()
end
continue
quit
