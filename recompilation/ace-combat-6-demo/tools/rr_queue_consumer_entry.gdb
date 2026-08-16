set pagination off
set confirm off
set debuginfod enabled off

python
import gdb

class QueueConsumerEntry(gdb.Breakpoint):
    """Read-only snapshot of the exact worker-side queue consumer entry."""

    def __init__(self):
        self.hits = 0
        super().__init__('__imp__sub_820FEFA8', gdb.BP_BREAKPOINT,
                         internal=False)
        self.silent = True

    def stop(self):
        try:
            ctx = int(gdb.parse_and_eval('$rdi'))
            base = int(gdb.parse_and_eval('$rsi'))
            bridge = int(gdb.parse_and_eval(
                '*(unsigned long*)($fs_base-0x290)'))
            tick = int(gdb.parse_and_eval(
                '*(unsigned long*)(%d+0x148)' % bridge))
            thread = int(gdb.parse_and_eval(
                '*(unsigned int*)($fs_base-0x588)'))
            inferior = gdb.selected_inferior()

            def gpr(index):
                offsets = {0: 8, 1: 16, 2: 24, 3: 0}
                if index not in offsets:
                    offsets[index] = 32 + (index - 4) * 8
                return int(gdb.parse_and_eval(
                    '*(unsigned long*)(%d+%d)' % (ctx, offsets[index])))

            def guest_u32(address, offset):
                if address == 0 or address > 0xFFFFFFFF - offset - 4:
                    return None
                try:
                    raw = bytes(inferior.read_memory(
                        base + address + offset, 4))
                    return int.from_bytes(raw, 'big')
                except gdb.error:
                    return None

            r = {i: gpr(i) & 0xFFFFFFFF for i in range(3, 8)}
            r31 = gpr(31) & 0xFFFFFFFF
            object_address = r[3]
            fields = {off: guest_u32(object_address, off)
                      for off in (0, 4, 8, 20, 24, 64, 68, 72, 76, 80)}
            lr = int(gdb.parse_and_eval(
                '*(unsigned long*)(%d+0x100)' % ctx)) & 0xFFFFFFFF
            print('AC6_QUEUE_CONSUMER_ENTRY hit=%d tick=%d thread=%d '
                  'pc=0x%08x lr=0x%08x r3=0x%08x r4=0x%08x r5=0x%08x '
                  'r6=0x%08x r7=0x%08x r31=0x%08x fields=%s' %
                  (self.hits, tick, thread,
                   int(gdb.parse_and_eval('$pc')) & 0xFFFFFFFF, lr,
                   r[3], r[4], r[5], r[6], r[7], r31,
                   ','.join('0x%02x=%s' %
                            (off, 'none' if value is None else
                             '0x%08x' % value)
                            for off, value in fields.items())))
            self.hits += 1
            if self.hits >= 16:
                self.enabled = False
        except gdb.error as exc:
            print('AC6_QUEUE_CONSUMER_ENTRY_ERROR %s' % exc)
        return False

QueueConsumerEntry()
end
continue
quit
