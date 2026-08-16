#include "ac6demo/content.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <system_error>

namespace ac6demo {

namespace {

void set_failure(std::string* failure, std::string message) {
  if (failure != nullptr) {
    *failure = std::move(message);
  }
}

[[nodiscard]] std::filesystem::path unique_sibling(const std::filesystem::path& path,
                                                    std::string_view suffix) {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  return path.parent_path() /
         (path.filename().string() + "." + std::string(suffix) + "." +
          std::to_string(stamp));
}

}  // namespace

bool verify_qualified_file(const std::filesystem::path& path, const QualifiedFile& expected,
                           std::string* failure) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(path, error)) {
    set_failure(failure, "missing qualified file: " + path.string());
    return false;
  }
  const auto size = std::filesystem::file_size(path, error);
  if (error || size != expected.size) {
    set_failure(failure, "size mismatch for " + std::string(expected.name));
    return false;
  }
  try {
    if (Sha256::file(path) != expected.sha256) {
      set_failure(failure, "SHA-256 mismatch for " + std::string(expected.name));
      return false;
    }
  } catch (const std::exception& exception) {
    set_failure(failure, exception.what());
    return false;
  }
  return true;
}

bool DemoStore::verify(const std::filesystem::path& store, std::string* failure) {
  std::error_code error;
  if (!std::filesystem::is_directory(store, error)) {
    set_failure(failure, "demo store is not a directory: " + store.string());
    return false;
  }
  const auto marker = store / ".ac6-demo-store";
  std::ifstream marker_stream(marker, std::ios::binary);
  const std::string expected_marker = "AC6-DEMO-STORE-v1\n" +
                                      std::string(kQualifiedXexSha256) + "\n";
  const std::string actual_marker((std::istreambuf_iterator<char>(marker_stream)),
                                  std::istreambuf_iterator<char>());
  if (!marker_stream.is_open() || marker_stream.bad() || actual_marker != expected_marker) {
    set_failure(failure, "demo store marker is missing or has the wrong identity");
    return false;
  }
  for (const QualifiedFile& file : qualified_files()) {
    if (!verify_qualified_file(store / file.name, file, failure)) {
      return false;
    }
  }
  for (const auto& entry : std::filesystem::directory_iterator(store, error)) {
    if (error) {
      set_failure(failure, "unable to enumerate demo store: " + store.string());
      return false;
    }
    const auto name = entry.path().filename().string();
    if (name != ".ac6-demo-store") {
      bool qualified = false;
      for (const QualifiedFile& file : qualified_files()) {
        qualified = qualified || name == file.name;
      }
      if (!qualified) {
        set_failure(failure, "unexpected file in demo store: " + name);
        return false;
      }
    }
  }
  return true;
}

bool DemoStore::import_directory(const std::filesystem::path& source,
                                 const std::filesystem::path& destination,
                                 std::string* failure) {
  std::error_code error;
  if (!std::filesystem::is_directory(source, error)) {
    set_failure(failure, "import source is not a directory: " + source.string());
    return false;
  }

  const auto staging = unique_sibling(destination, "staging");
  std::filesystem::create_directories(staging, error);
  if (error) {
    set_failure(failure, "unable to create staging store: " + error.message());
    return false;
  }

  for (const QualifiedFile& file : qualified_files()) {
    const auto input = source / file.name;
    if (!verify_qualified_file(input, file, failure)) {
      std::filesystem::remove_all(staging, error);
      return false;
    }
    const auto output = staging / file.name;
    std::filesystem::copy_file(input, output, std::filesystem::copy_options::none, error);
    if (error) {
      set_failure(failure, "unable to stage " + std::string(file.name) + ": " + error.message());
      std::filesystem::remove_all(staging, error);
      return false;
    }
  }

  {
    std::ofstream marker(staging / ".ac6-demo-store", std::ios::binary);
    marker << "AC6-DEMO-STORE-v1\n" << kQualifiedXexSha256 << '\n';
    if (!marker) {
      set_failure(failure, "unable to write demo store marker");
      std::filesystem::remove_all(staging, error);
      return false;
    }
  }
  if (!verify(staging, failure)) {
    std::filesystem::remove_all(staging, error);
    return false;
  }

  std::filesystem::create_directories(destination.parent_path(), error);
  if (error) {
    set_failure(failure, "unable to create store parent: " + error.message());
    std::filesystem::remove_all(staging, error);
    return false;
  }
  if (std::filesystem::exists(destination, error)) {
    const auto backup = unique_sibling(destination, "backup");
    std::filesystem::rename(destination, backup, error);
    if (error) {
      set_failure(failure, "unable to preserve existing demo store: " + error.message());
      std::filesystem::remove_all(staging, error);
      return false;
    }
  }
  std::filesystem::rename(staging, destination, error);
  if (error) {
    set_failure(failure, "unable to publish demo store atomically: " + error.message());
    std::filesystem::remove_all(staging, error);
    return false;
  }
  return true;
}

std::filesystem::path DemoStore::default_path() {
  const char* xdg_data = std::getenv("XDG_DATA_HOME");
  if (xdg_data != nullptr && *xdg_data != '\0') {
    return std::filesystem::path(xdg_data) / "ac6-demo-recomp" / "demo";
  }
  const char* user_home = std::getenv("HOME");
  if (user_home != nullptr && *user_home != '\0') {
    return std::filesystem::path(user_home) / ".local" / "share" / "ac6-demo-recomp" /
           "demo";
  }
  return std::filesystem::current_path() / ".ac6-demo-store";
}

VfsMount::VfsMount(std::filesystem::path store) : store_(std::move(store)) {
  std::string failure;
  if (!DemoStore::verify(store_, &failure)) {
    throw RuntimeTrap("cannot mount demo VFS: " + failure);
  }
}

std::optional<std::filesystem::path> VfsMount::resolve_if_qualified(
    std::string_view xbox_path) const {
  std::string normalized(xbox_path);
  for (char& character : normalized) {
    if (character == '\\') {
      character = '/';
    }
  }
  constexpr std::string_view prefix = "game:/";
  if (!normalized.starts_with(prefix)) {
    throw RuntimeTrap("VFS path outside qualified game namespace: " + normalized);
  }
  const std::string_view name(normalized.data() + prefix.size(),
                              normalized.size() - prefix.size());
  for (const QualifiedFile& file : qualified_files()) {
    if (name == file.name) {
      return store_ / std::string(file.name);
    }
  }
  return std::nullopt;
}

std::filesystem::path VfsMount::resolve(std::string_view xbox_path) const {
  const auto resolved = resolve_if_qualified(xbox_path);
  if (resolved.has_value()) {
    return *resolved;
  }
  std::string normalized(xbox_path);
  for (char& character : normalized) {
    if (character == '\\') {
      character = '/';
    }
  }
  constexpr std::string_view prefix = "game:/";
  const std::string_view name(normalized.data() + prefix.size(),
                              normalized.size() - prefix.size());
  throw RuntimeTrap("VFS path is not part of the qualified demo: " + std::string(name));
}

}  // namespace ac6demo
