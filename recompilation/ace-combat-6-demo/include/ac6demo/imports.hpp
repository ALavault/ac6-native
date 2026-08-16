#pragma once

#include "ac6demo/ppc.hpp"
#include "ac6demo/runtime_error.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ac6demo {

struct ImportKey final {
  std::string module;
  std::uint16_t ordinal{};

  bool operator==(const ImportKey&) const = default;
};

struct ImportKeyHash final {
  [[nodiscard]] std::size_t operator()(const ImportKey& key) const noexcept;
};

struct ImportRecord final {
  ImportKey key;
  std::uint32_t thunk{};
};

class ImportRegistry final {
 public:
  using Handler = std::function<void(PpcContext&, std::uint32_t lr, std::uint64_t tick)>;

  void add(ImportRecord record);
  void implement(const ImportKey& key, Handler handler);
  void invoke(const ImportKey& key, PpcContext& context, std::uint32_t lr,
              std::uint64_t tick) const;

  [[nodiscard]] std::size_t size() const noexcept { return records_.size(); }
  [[nodiscard]] std::size_t implemented() const noexcept { return handlers_.size(); }

 private:
  std::unordered_map<ImportKey, ImportRecord, ImportKeyHash> records_;
  std::unordered_map<ImportKey, Handler, ImportKeyHash> handlers_;
};

class XamPersistence final {
 public:
  explicit XamPersistence(std::filesystem::path root) : root_(std::move(root)) {}

  void write_public(std::string_view name, std::string_view payload);
  [[nodiscard]] std::string read_public(std::string_view name) const;

 private:
  static void validate_name(std::string_view name);
  std::filesystem::path root_;
};

}  // namespace ac6demo
