#pragma once

#include "ac6/retail_content.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace ac6 {

enum class RetailResourceKind : std::uint8_t {
  Unknown = 0,
  Fhm = 1,
  Nfic = 2,
  Scene = 3,
  Swg = 4,
  Mate = 5,
  Ndxr = 6,
  Ntxr = 7,
  Riff = 8,
  Asf = 9,
};

struct RetailResourceNode final {
  std::uint32_t entry{};
  std::uint32_t parent{UINT32_MAX};
  std::uint32_t child_slot{UINT32_MAX};
  std::uint32_t offset{};
  std::uint32_t length{};
  std::uint32_t gidx{};
  std::uint8_t depth{};
  RetailResourceKind kind{RetailResourceKind::Unknown};
};

struct RetailResourceRelation final {
  std::uint32_t from{};
  std::uint32_t target_gidx{};
  std::uint32_t target_node{UINT32_MAX};
  std::uint8_t kind{}; // 1 = material/NDXR texture reference
};

class RetailResourceGraph final {
 public:
  bool open(const std::filesystem::path& cache_root,
            const Sha256Digest& content_index_sha256);
  bool valid() const noexcept { return valid_; }
  const char* detail() const noexcept { return detail_; }
  const Sha256Digest& manifest_sha256() const noexcept { return manifest_sha256_; }
  const std::vector<RetailResourceNode>& nodes() const noexcept { return nodes_; }
  const std::vector<RetailResourceRelation>& relations() const noexcept {
    return relations_;
  }

 private:
  bool valid_{};
  const char* detail_{"resource graph is not open"};
  Sha256Digest manifest_sha256_{};
  std::vector<RetailResourceNode> nodes_;
  std::vector<RetailResourceRelation> relations_;
};

bool publish_retail_resource_graph(
    const std::filesystem::path& cache_root,
    const std::filesystem::path& staging_root,
    const Sha256Digest& content_index_sha256,
    const std::vector<RetailContentRecord>& records,
    Sha256Digest& graph_manifest_sha256,
    std::string& detail);

}  // namespace ac6
