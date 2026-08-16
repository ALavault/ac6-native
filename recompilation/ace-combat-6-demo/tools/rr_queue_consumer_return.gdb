set pagination off
set confirm off
set debuginfod enabled off

python
import gdb

class ConsumerReturn(gdb.Breakpoint):
    def __init__(self, ctx, hit, return_address):
        self.ctx = ctx
        self.hit = hit
        super().__init__('*0x%x' % return_address, gdb.BP_BREAKPOINT,
                         internal=False)
        self.silent = True

    def stop(self):
        try:
            offsets = {0: 8, 1: 16, 2: 24, 3: 0}
            for index in range(4, 32):
                offsets[index] = 32 + (index - 4) * 8
            def gpr(index):
                return int(gdb.parse_and_eval(
                    '*(unsigned long*)(%d+%d)' %
                    (self.ctx, offsets[index]))) & 0xFFFFFFFF
            lr = int(gdb.parse_and_eval(
                '*(unsigned long*)(%d+0x100)' % self.ctx)) & 0xFFFFFFFF
            print('AC6_QUEUE_CONSUMER_RETURN hit=%d host_pc=0x%016x '
                  'lr=0x%08x r3=0x%08x r4=0x%08x r5=0x%08x r6=0x%08x '
                  'r7=0x%08x r31=0x%08x' %
                  (self.hit, int(gdb.parse_and_eval('$pc')),
                   lr, gpr(3), gpr(4), gpr(5), gpr(6), gpr(7), gpr(31)))
        except gdb.error as exc:
            print('AC6_QUEUE_CONSUMER_RETURN_ERROR %s' % exc)
        return False

class ConsumerEntry(gdb.Breakpoint):
    def __init__(self):
        self.hits = 0
        super().__init__('__imp__sub_820FEFA8', gdb.BP_BREAKPOINT,
                         internal=False)
        self.silent = True

    def stop(self):
        try:
            ctx = int(gdb.parse_and_eval('$rdi'))
            ret = int(gdb.parse_and_eval('*(unsigned long*)$rsp'))
            offsets = {0: 8, 1: 16, 2: 24, 3: 0}
            for index in range(4, 32):
                offsets[index] = 32 + (index - 4) * 8
            def raw(index):
                return int(gdb.parse_and_eval(
                    '*(unsigned long*)(%d+%d)' % (ctx, offsets[index]))) & 0xFFFFFFFF
            print('AC6_QUEUE_CONSUMER_ENTRY_RETURN_ARM hit=%d '
                  'host_return=0x%016x lr=0x%08x r3=0x%08x r31=0x%08x' %
                  (self.hits, ret,
                   int(gdb.parse_and_eval(
                       '*(unsigned long*)(%d+0x100)' % ctx)) & 0xFFFFFFFF,
                   raw(3), raw(31)))
            if ret != 0:
                ConsumerReturn(ctx, self.hits, ret)
            else:
                print('AC6_QUEUE_CONSUMER_ENTRY_RETURN_NO_HOST_RETURN')
            self.hits += 1
            if self.hits >= 8:
                self.enabled = False
        except gdb.error as exc:
            print('AC6_QUEUE_CONSUMER_ENTRY_RETURN_ERROR %s' % exc)
        return False

ConsumerEntry()
end
continue
quit
