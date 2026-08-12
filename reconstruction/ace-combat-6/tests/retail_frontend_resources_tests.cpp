#include "ac6/retail_frontend_resources.h"
#include "ac6/frontend_runtime.h"

#include <array>
#include <cstdio>
#include <cstdlib>

int main() {
  const char* cache_root = std::getenv("AC6_RETAIL_CACHE");
  if (cache_root == nullptr || *cache_root == '\0') return 77;
  ac6::RetailContentStore store;
  if (!store.open(cache_root)) {
    std::fprintf(stderr, "cache_open_failed:%s:%s\n",
                 ac6::retail_content_error_name(store.error()), store.detail().c_str());
    return 1;
  }
  const auto resources = ac6::retail::RetailFrontendResources::open(store);
  if (!resources.has_value() || !resources->complete()) return 2;
  constexpr std::array<std::uint32_t, 5> kExpectedLocaleEntries{4, 7, 5, 6, 8};
  for (std::uint32_t slot = 0; slot < kExpectedLocaleEntries.size(); ++slot) {
    const auto entry = resources->locale_data_table_entry(slot);
    if (!entry.has_value() || *entry != kExpectedLocaleEntries[slot]) return 3;
    if (!resources->has_locale_slot(slot)) return 3;
    for (std::uint32_t difficulty = 0; difficulty <=
             static_cast<std::uint32_t>(ac6::FrontendDifficulty::Hard);
         ++difficulty) {
      for (std::uint32_t controls = 0; controls <=
               static_cast<std::uint32_t>(ac6::FrontendControls::Expert);
           ++controls) {
        ac6::FrontendController frontend;
        if (!frontend.configure(
                {static_cast<ac6::FrontendDifficulty>(difficulty),
                 static_cast<ac6::FrontendControls>(controls),
                 static_cast<ac6::FrontendLanguage>(slot)}, *resources)) {
          return 4;
        }
      }
    }
  }
  if (resources->locale_data_table_entry(kExpectedLocaleEntries.size()).has_value() ||
      resources->locale_data_table_entry(0xffffffffu).has_value() ||
      resources->has_locale_slot(kExpectedLocaleEntries.size()) ||
      resources->has_locale_slot(0xffffffffu)) {
    return 5;
  }
  std::fprintf(stdout, "frontend_fonts=pass entries=7 locales=5 index_sha256=%s\n",
               ac6::sha256_hex(resources->content_index_sha256()).c_str());
  return 0;
}
