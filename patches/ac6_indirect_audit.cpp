#include <rex/logging.h>
#include <rex/logging/api.h>

#include <cstdint>
#include <mutex>
#include <set>

// Records every indirect-call target the recompiler never registered. Each one
// is a real function entry the boundary analysis absorbed into a neighbour, and
// each corrupts the guest context when called, because the faulting call is
// swallowed by the MMIO SIGSEGV handler.
extern "C" void rex_report_unregistered_indirect_target(uint32_t guest_address) {
    static std::mutex mutex;
    static std::set<uint32_t> seen;

    bool first = false;
    {
        std::scoped_lock lock(mutex);
        first = seen.insert(guest_address).second;
    }
    if (first) {
        REXLOG_ERROR("UNREGISTERED indirect target {:#010x} -- missing [functions] entry",
                     guest_address);
    }
}
