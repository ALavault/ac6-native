#include "ac6demo/imports.hpp"
#include "ac6demo/hash.hpp"

#include <fstream>
#include <utility>

namespace ac6demo {

std::size_t ImportKeyHash::operator()(const ImportKey& key) const noexcept {
  std::size_t value = std::hash<std::string>{}(key.module);
  value ^= static_cast<std::size_t>(key.ordinal) + 0x9e3779b9U + (value << 6U) + (value >> 2U);
  return value;
}

void ImportRegistry::add(ImportRecord record) {
  if (record.key.module.empty() || record.key.ordinal == 0U || record.thunk == 0U) {
    throw std::invalid_argument("invalid XEX import record");
  }
  records_.insert_or_assign(record.key, std::move(record));
}

void ImportRegistry::implement(const ImportKey& key, Handler handler) {
  if (records_.find(key) == records_.end()) {
    throw std::invalid_argument("implementation registered for unknown XEX import");
  }
  if (!handler) {
    throw std::invalid_argument("empty XEX import implementation");
  }
  handlers_.insert_or_assign(key, std::move(handler));
}

void ImportRegistry::invoke(const ImportKey& key, PpcContext& context, std::uint32_t lr,
                            std::uint64_t tick) const {
  const auto record = records_.find(key);
  if (record == records_.end()) {
    throw RuntimeTrap("import not present in qualified XEX: " + key.module + " ordinal " +
                          std::to_string(key.ordinal),
                      tick, lr);
  }
  const auto handler = handlers_.find(key);
  if (handler == handlers_.end()) {
    throw RuntimeTrap("unimplemented import " + key.module + " ordinal " +
                          std::to_string(key.ordinal) + " LR=0x" +
                          hex_u64(lr).substr(8),
                      tick, lr, record->second.thunk);
  }
  handler->second(context, lr, tick);
}

void XamPersistence::validate_name(std::string_view name) {
  if (name.empty() || name.find('/') != std::string_view::npos ||
      name.find('\\') != std::string_view::npos || name.find("..") != std::string_view::npos ||
      name.front() == '.') {
    throw RuntimeTrap("XAM persistence name outside public namespace");
  }
}

void XamPersistence::write_public(std::string_view name, std::string_view payload) {
  validate_name(name);
  std::error_code error;
  std::filesystem::create_directories(root_, error);
  if (error) {
    throw RuntimeTrap("XAM persistence directory creation failed: " + error.message());
  }
  const auto target = root_ / std::string(name);
  const auto temporary = target.string() + ".tmp";
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    if (!output) {
      throw RuntimeTrap("XAM persistence write failed");
    }
  }
  std::filesystem::rename(temporary, target, error);
  if (error) {
    std::filesystem::remove(temporary, error);
    throw RuntimeTrap("XAM persistence publish failed: " + error.message());
  }
}

std::string XamPersistence::read_public(std::string_view name) const {
  validate_name(name);
  std::ifstream input(root_ / std::string(name), std::ios::binary);
  if (!input) {
    return {};
  }
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

}  // namespace ac6demo
