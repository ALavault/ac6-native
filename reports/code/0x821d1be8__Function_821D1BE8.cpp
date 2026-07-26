#include <cstdint>

struct Ace6TreeEntry {
    virtual std::int32_t GetId() const = 0;
};

struct Ace6TreeEntryLink {
    Ace6TreeEntry* entry{};
    Ace6TreeEntryLink* next{};
};

Ace6TreeEntryLink* Function_821D1BE8(const std::int32_t target_id,
                                     Ace6TreeEntryLink* link) {
    if (target_id != 0) {
        for (; link != nullptr; link = link->next) {
            if ((link->entry != nullptr) && (link->entry->GetId() == target_id)) {
                return link;
            }
        }
    }

    return nullptr;
}