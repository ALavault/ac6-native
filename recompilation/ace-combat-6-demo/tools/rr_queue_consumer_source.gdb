set pagination off
set confirm off
set debuginfod enabled off

python
import gdb

class QueueConsumerSource(gdb.Breakpoint):
    def __init__(self):
        self.hits = 0
        super().__init__('__imp__sub_820FFCA0', gdb.BP_BREAKPOINT,
                         internal=False)
        self.silent = True

    def stop(self):
        try:
            ctx = int(gdb.parse_and_eval('$rdi'))
            base = int(gdb.parse_and_eval('$rsi'))
            inferior = gdb.selected_inferior()
            def gpr(index):
                offsets = {0: 8, 1: 16, 2: 24, 3: 0}
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

            queue = gpr(3)
            def guest_u32(address):
                if address == 0 or address > 0xFFFFFFFF - 4:
                    return None
                try:
                    raw = bytes(inferior.read_memory(base + address, 4))
                    return int.from_bytes(raw, 'big')
                except gdb.error:
                    return None

            producer = guest_u32(queue + 24784)
            consumer = guest_u32(queue + 24788)
            source = None
            words = []
            if consumer is not None and consumer < 0x100000:
                source = (queue + consumer * 96 + 208) & 0xFFFFFFFF
                for offset in range(0, 0x60, 4):
                    value = guest_u32((source + offset) & 0xFFFFFFFF)
                    words.append('%02x:%s' %
                                 (offset, 'none' if value is None else
                                  '%08x' % value))
            lr = int(gdb.parse_and_eval(
                '*(unsigned long*)(%d+0x100)' % ctx)) & 0xFFFFFFFF
            print('AC6_QUEUE_CONSUMER_SOURCE hit=%d tick=%s thread=%s '
                  'pc=0x%08x lr=0x%08x queue=0x%08x producer=%s '
                  'consumer=%s source=%s words=%s' %
                  (self.hits, tick, thread,
                   int(gdb.parse_and_eval('$pc')) & 0xFFFFFFFF, lr, queue,
                   'none' if producer is None else '0x%08x' % producer,
                   'none' if consumer is None else '0x%08x' % consumer,
                   'none' if source is None else '0x%08x' % source,
                   ','.join(words)))
            self.hits += 1
            self.enabled = False
        except gdb.error as exc:
            print('AC6_QUEUE_CONSUMER_SOURCE_ERROR %s' % exc)
        return False

QueueConsumerSource()
end
continue
quit
