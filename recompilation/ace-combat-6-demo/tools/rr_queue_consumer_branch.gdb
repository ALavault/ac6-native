set pagination off
set confirm off
set debuginfod enabled off

python
import gdb

class QueueConsumerBranch(gdb.Breakpoint):
    def __init__(self, symbol):
        self.symbol = symbol
        self.hits = 0
        super().__init__(symbol, gdb.BP_BREAKPOINT, internal=False)
        self.silent = True

    def stop(self):
        try:
            ctx = int(gdb.parse_and_eval('$rdi'))
            offsets = {0: 8, 1: 16, 2: 24, 3: 0}
            def gpr(index):
                offset = offsets.get(index, 32 + (index - 4) * 8)
                return int(gdb.parse_and_eval(
                    '*(unsigned long*)(%d+%d)' % (ctx, offset))) & 0xFFFFFFFF
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
            print('AC6_QUEUE_BRANCH symbol=%s hit=%d tick=%s thread=%s '
                  'r1=0x%08x r3=0x%08x r4=0x%08x r5=0x%08x '
                  'r6=0x%08x r7=0x%08x r31=0x%08x lr=0x%08x' %
                  (self.symbol, self.hits, tick, thread, gpr(1), gpr(3),
                   gpr(4), gpr(5), gpr(6), gpr(7), gpr(31),
                   int(gdb.parse_and_eval(
                       '*(unsigned long*)(%d+0x100)' % ctx)) & 0xFFFFFFFF))
            self.hits += 1
            if self.hits >= 8:
                self.enabled = False
        except gdb.error as exc:
            print('AC6_QUEUE_BRANCH_ERROR symbol=%s %s' % (self.symbol, exc))
        return False

for symbol in ('__imp__sub_820FEA88', '__imp__sub_8226D6A0',
               '__imp__sub_8226E398'):
    QueueConsumerBranch(symbol)
end
continue
quit
