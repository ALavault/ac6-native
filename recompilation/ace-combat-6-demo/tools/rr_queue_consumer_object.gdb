set pagination off
set confirm off
set debuginfod enabled off

python
import gdb

class QueueConsumerObject(gdb.Breakpoint):
    def __init__(self):
        self.hits = 0
        super().__init__('__imp__sub_820FEFA8', gdb.BP_BREAKPOINT,
                         internal=False)
        self.silent = True

    def stop(self):
        try:
            ctx = int(gdb.parse_and_eval('$rdi'))
            base = int(gdb.parse_and_eval('$rsi'))
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
            inferior = gdb.selected_inferior()

            def gpr(index):
                offsets = {0: 8, 1: 16, 2: 24, 3: 0}
                offset = offsets.get(index, 32 + (index - 4) * 8)
                return int(gdb.parse_and_eval(
                    '*(unsigned long*)(%d+%d)' % (ctx, offset))) & 0xFFFFFFFF

            object_address = gpr(3)
            if object_address == 0 or object_address > 0xFFFFFFFF - 0x60:
                raise gdb.error('invalid object pointer 0x%08x' % object_address)
            values = []
            for offset in range(0, 0x60, 4):
                raw = bytes(inferior.read_memory(
                    base + object_address + offset, 4))
                values.append('%02x:%08x' %
                              (offset, int.from_bytes(raw, 'big')))
            lr = int(gdb.parse_and_eval(
                '*(unsigned long*)(%d+0x100)' % ctx)) & 0xFFFFFFFF
            print('AC6_QUEUE_CONSUMER_OBJECT hit=%d tick=%s thread=%s '
                  'pc=0x%08x lr=0x%08x r3=0x%08x r31=0x%08x fields=%s' %
                  (self.hits, tick, thread,
                   int(gdb.parse_and_eval('$pc')) & 0xFFFFFFFF, lr,
                   object_address, gpr(31), ','.join(values)))
            self.hits += 1
            if self.hits >= 8:
                self.enabled = False
        except gdb.error as exc:
            print('AC6_QUEUE_CONSUMER_OBJECT_ERROR %s' % exc)
        return False

QueueConsumerObject()
end
continue
quit
