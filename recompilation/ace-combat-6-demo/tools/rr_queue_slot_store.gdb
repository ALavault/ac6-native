set pagination off
set confirm off
set debuginfod enabled off

python
import gdb

class QueueSlotStore(gdb.Breakpoint):
    BEGIN = 0x82386D90
    END = 0x82386DF0

    def __init__(self, symbol, width):
        self.symbol = symbol
        self.width = width
        self.hits = 0
        super().__init__(symbol, gdb.BP_BREAKPOINT, internal=False)
        self.silent = True

    def stop(self):
        try:
            address = int(gdb.parse_and_eval('$rdx')) & 0xFFFFFFFF
            if address < self.BEGIN or address >= self.END:
                return False
            if address + self.width > self.END:
                print('AC6_QUEUE_SLOT_STORE_OUT_OF_RANGE symbol=%s '
                      'address=0x%08x width=%d' %
                      (self.symbol, address, self.width))
                self.enabled = False
                return False
            ctx = int(gdb.parse_and_eval('$rdi'))
            lr = int(gdb.parse_and_eval(
                '*(unsigned long*)(%d+0x100)' % ctx)) & 0xFFFFFFFF
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
            value = 'na'
            if self.width <= 8:
                value = '0x%016x' % (int(gdb.parse_and_eval('$rcx')) &
                                     0xFFFFFFFFFFFFFFFF)
            print('AC6_QUEUE_SLOT_STORE symbol=%s hit=%d address=0x%08x '
                  'width=%d value=%s tick=%s thread=%s lr=0x%08x' %
                  (self.symbol, self.hits, address, self.width, value,
                   tick, thread, lr))
            self.hits += 1
            if self.hits >= 128:
                self.enabled = False
        except gdb.error as exc:
            print('AC6_QUEUE_SLOT_STORE_ERROR symbol=%s %s' %
                  (self.symbol, exc))
        return False

for symbol, width in (
        ('AC6_PPC_STORE_U8', 1),
        ('AC6_PPC_STORE_U16', 2),
        ('AC6_PPC_STORE_U32', 4),
        ('AC6_PPC_STORE_U64', 8),
        ('AC6_PPC_STORE_U128', 16)):
    QueueSlotStore(symbol, width)
end
continue
quit
