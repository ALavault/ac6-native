#include "ac6/retail_resource_graph.h"

#include "ac6/ntxr_texture.h"
#include "ac6/retail_ndxr_container.h"
#include "ac6/retail_fhm_view.h"
#include "ac6/sha256.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unistd.h>
#include <fcntl.h>

namespace ac6 {
namespace {

constexpr std::array<std::uint8_t, 8> kMagic{'A','C','6','G','R','A','P','H'};
constexpr std::uint32_t kVersion = 2;
constexpr std::array<std::uint8_t, 8> kCurrentMagic{'A','C','6','G','C','U','R',0};
constexpr std::uint32_t kNodeSize = 28;
constexpr std::uint32_t kHeaderSize = 60;
constexpr std::uint32_t kCurrentSize = 48;
constexpr std::uint32_t kMaxNodes = 200000;
constexpr std::uint32_t kRelationSize = 16;

void u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value >> 24u));
  out.push_back(static_cast<std::uint8_t>(value >> 16u));
  out.push_back(static_cast<std::uint8_t>(value >> 8u));
  out.push_back(static_cast<std::uint8_t>(value));
}

std::uint32_t u32(const std::vector<std::uint8_t>& in, std::size_t at) {
  return (static_cast<std::uint32_t>(in[at]) << 24u) |
         (static_cast<std::uint32_t>(in[at + 1]) << 16u) |
         (static_cast<std::uint32_t>(in[at + 2]) << 8u) | in[at + 3];
}

bool write_sync(const std::filesystem::path& path,
                const std::vector<std::uint8_t>& bytes) {
  const int descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (descriptor < 0) return false;
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const ssize_t count = ::write(descriptor, bytes.data() + offset, bytes.size() - offset);
    if (count <= 0) { ::close(descriptor); return false; }
    offset += static_cast<std::size_t>(count);
  }
  const bool synced = ::fsync(descriptor) == 0;
  ::close(descriptor);
  return synced;
}

bool replace_file(const std::filesystem::path& temporary,
                  const std::filesystem::path& destination) {
  std::error_code error;
  std::filesystem::rename(temporary, destination, error);
  if (!error) return true;
  if (error != std::errc::file_exists) return false;
  std::filesystem::remove(temporary, error);
  return !error;
}

std::filesystem::path blob_path(const std::filesystem::path& cache,
                                const Sha256Digest& digest) {
  const auto hex = sha256_hex(digest);
  return cache / "blobs/sha256" / hex.substr(0, 2) / hex;
}

RetailResourceKind kind_of(std::span<const std::uint8_t> bytes) noexcept {
  if (bytes.size() < 4) return RetailResourceKind::Unknown;
  const auto at = bytes.data();
  if (std::equal(at, at + 4, "FHM ")) return RetailResourceKind::Fhm;
  if (std::equal(at, at + 4, "NFIC")) return RetailResourceKind::Nfic;
  if (std::equal(at, at + 4, "Scen")) return RetailResourceKind::Scene;
  if (std::equal(at, at + 4, "SWG\0")) return RetailResourceKind::Swg;
  if (std::equal(at, at + 4, "MATE")) return RetailResourceKind::Mate;
  if (std::equal(at, at + 4, "NDXR")) return RetailResourceKind::Ndxr;
  if (std::equal(at, at + 4, "NTXR")) return RetailResourceKind::Ntxr;
  if (std::equal(at, at + 4, "RIFF")) return RetailResourceKind::Riff;
  if (std::equal(at, at + 4, "ASF\0")) return RetailResourceKind::Asf;
  return RetailResourceKind::Unknown;
}

bool walk(std::span<const std::uint8_t> bytes, std::uint32_t entry,
          std::uint32_t parent, std::uint32_t slot, std::uint8_t depth,
          std::vector<RetailResourceNode>& nodes,
          std::vector<RetailResourceRelation>& relations, std::string& detail) {
  if (depth > 32 || nodes.size() >= kMaxNodes) {
    detail = "resource graph nesting or node limit exceeded";
    return false;
  }
  const auto kind = kind_of(bytes);
  RetailResourceNode node;
  node.entry = entry;
  node.parent = parent;
  node.child_slot = slot;
  node.offset = 0;
  node.length = static_cast<std::uint32_t>(bytes.size());
  node.depth = depth;
  node.kind = kind;
  if (kind == RetailResourceKind::Ntxr) {
    node.gidx = retail::ntxr_gidx_identifier(bytes.data(), bytes.size()).value_or(0);
  }
  const std::uint32_t node_index = static_cast<std::uint32_t>(nodes.size());
  nodes.push_back(node);
  if (kind == RetailResourceKind::Ndxr) {
    retail::NdxrRefusal refusal = retail::NdxrRefusal::kNone;
    const auto container = retail::NdxrContainer::Open(bytes.data(), bytes.size(), &refusal);
    if (container.has_value()) {
      for (std::uint16_t record_index = 0; record_index < container->record_count();
           ++record_index) {
        const auto record = container->Record(record_index);
        if (!record.has_value()) continue;
        for (std::uint16_t descriptor = 0; descriptor < record->descriptor_count; ++descriptor) {
          for (unsigned slot_index = 0; slot_index < 4; ++slot_index) {
            const auto material = container->Material(*record, descriptor, slot_index);
            if (!material.has_value()) continue;
            for (std::uint16_t texture = 0; texture < material->texture_count; ++texture) {
              const auto reference = container->TextureRef(*material, texture);
              if (reference.has_value() && reference->texture_id != 0) {
                relations.push_back({node_index, reference->texture_id,
                                     UINT32_MAX, 1});
              }
            }
          }
        }
      }
    }
  }
  if (kind != RetailResourceKind::Fhm) return true;
  const auto view = retail::RetailFhmView::open(bytes);
  if (!view.has_value()) {
    // Retail contains empty FHM sentinels inside a few resource families.
    // They are graph leaves, not permission to invent child semantics.
    if (bytes.size() >= 20 && bytes[4] == 1 && bytes[5] == 1 &&
        bytes[6] == 0 && bytes[7] == 0x10 &&
        bytes[16] == 0 && bytes[17] == 0 && bytes[18] == 0 && bytes[19] == 0) {
      return true;
    }
    detail = "invalid FHM in resource graph at DATA.TBL entry " + std::to_string(entry);
    return false;
  }
  for (std::uint32_t child = 0; child < view->child_count(); ++child) {
    const auto child_bytes = view->child(child);
    if (!child_bytes.has_value()) continue;
    const std::uint32_t child_node = static_cast<std::uint32_t>(nodes.size());
    if (!walk(*child_bytes, entry, node_index, child,
              static_cast<std::uint8_t>(depth + 1), nodes, relations, detail)) return false;
    nodes[child_node].offset = static_cast<std::uint32_t>(
        child_bytes->data() - bytes.data());
    nodes[child_node].length = static_cast<std::uint32_t>(child_bytes->size());
  }
  return true;
}

bool parse_graph(const std::vector<std::uint8_t>& bytes,
                 const Sha256Digest& expected_index,
                 std::vector<RetailResourceNode>& nodes,
                 std::vector<RetailResourceRelation>& relations) {
  if (bytes.size() < kHeaderSize ||
      !std::equal(kMagic.begin(), kMagic.end(), bytes.begin()) ||
      u32(bytes, 8) != kVersion || u32(bytes, 12) != kHeaderSize ||
      u32(bytes, 16) != kNodeSize || u32(bytes, 20) > kMaxNodes) return false;
  if (!std::equal(expected_index.begin(), expected_index.end(), bytes.begin() + 28)) return false;
  const auto count = u32(bytes, 20);
  const auto relation_count = u32(bytes, 24);
  if (relation_count > 2000000u) return false;
  if (bytes.size() != kHeaderSize + static_cast<std::size_t>(count) * kNodeSize +
                            static_cast<std::size_t>(relation_count) * kRelationSize) return false;
  nodes.clear(); nodes.reserve(count);
  std::size_t at = kHeaderSize;
  for (std::uint32_t i = 0; i < count; ++i, at += kNodeSize) {
    RetailResourceNode node;
    node.entry = u32(bytes, at); node.parent = u32(bytes, at + 4);
    node.child_slot = u32(bytes, at + 8); node.offset = u32(bytes, at + 12);
    node.length = u32(bytes, at + 16); node.gidx = u32(bytes, at + 20);
    node.depth = bytes[at + 24]; node.kind = static_cast<RetailResourceKind>(bytes[at + 25]);
    if (node.parent != UINT32_MAX && node.parent >= count) return false;
    nodes.push_back(node);
  }
  relations.clear(); relations.reserve(relation_count);
  std::unordered_map<std::uint32_t, std::uint32_t> texture_nodes;
  texture_nodes.reserve(count);
  for (std::uint32_t node = 0; node < count; ++node) {
    if (nodes[node].kind == RetailResourceKind::Ntxr && nodes[node].gidx != 0) {
      texture_nodes.emplace(nodes[node].gidx, node);
    }
  }
  for (std::uint32_t i = 0; i < relation_count; ++i, at += kRelationSize) {
    RetailResourceRelation relation;
    relation.from = u32(bytes, at);
    relation.target_gidx = u32(bytes, at + 4);
    relation.target_node = u32(bytes, at + 8);
    relation.kind = static_cast<std::uint8_t>(u32(bytes, at + 12));
    if (relation.from >= count || relation.kind == 0) return false;
    relation.target_node = UINT32_MAX;
    const auto found = texture_nodes.find(relation.target_gidx);
    if (found != texture_nodes.end()) relation.target_node = found->second;
    relations.push_back(relation);
  }
  return true;
}

}  // namespace

bool publish_retail_resource_graph(
    const std::filesystem::path& cache_root,
    const std::filesystem::path& staging_root,
    const Sha256Digest& content_index_sha256,
    const std::vector<RetailContentRecord>& records,
    Sha256Digest& graph_manifest_sha256,
    std::string& detail) {
  std::vector<RetailResourceNode> nodes;
  std::vector<RetailResourceRelation> relations;
  for (const auto& record : records) {
    std::vector<std::uint8_t> bytes;
    const auto path = blob_path(cache_root, record.payload_sha256);
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size != record.payload_size || size > UINT32_MAX) {
      detail = "resource graph payload blob is missing or oversized";
      return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) { detail = "resource graph payload cannot be opened"; return false; }
    bytes.resize(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    if (!input) { detail = "resource graph payload cannot be read"; return false; }
    if (!walk(bytes, record.data_table_index, UINT32_MAX, UINT32_MAX, 0,
              nodes, relations, detail)) return false;
  }
  std::vector<std::uint8_t> manifest;
  manifest.insert(manifest.end(), kMagic.begin(), kMagic.end());
  u32(manifest, kVersion); u32(manifest, kHeaderSize); u32(manifest, kNodeSize);
  u32(manifest, static_cast<std::uint32_t>(nodes.size()));
  u32(manifest, static_cast<std::uint32_t>(relations.size()));
  manifest.insert(manifest.end(), content_index_sha256.begin(), content_index_sha256.end());
  manifest.resize(kHeaderSize, 0);
  for (const auto& node : nodes) {
    u32(manifest, node.entry); u32(manifest, node.parent); u32(manifest, node.child_slot);
    u32(manifest, node.offset); u32(manifest, node.length); u32(manifest, node.gidx);
    manifest.push_back(node.depth); manifest.push_back(static_cast<std::uint8_t>(node.kind));
    manifest.push_back(0); manifest.push_back(0);
  }
  for (const auto& relation : relations) {
    u32(manifest, relation.from); u32(manifest, relation.target_gidx);
    u32(manifest, relation.target_node); u32(manifest, relation.kind);
  }
  graph_manifest_sha256 = sha256_bytes(manifest);
  std::error_code error;
  std::filesystem::create_directories(cache_root / "graph/manifests", error);
  if (error) { detail = "cannot create resource graph directory"; return false; }
  const auto path = cache_root / "graph/manifests" /
      (sha256_hex(graph_manifest_sha256) + ".ac6graph");
  if (!std::filesystem::exists(path) &&
      (!write_sync(staging_root / "resource-graph", manifest) ||
       !replace_file(staging_root / "resource-graph", path))) {
    detail = "cannot publish resource graph manifest";
    return false;
  }
  std::vector<std::uint8_t> current;
  current.insert(current.end(), kCurrentMagic.begin(), kCurrentMagic.end());
  u32(current, kVersion); u32(current, kCurrentSize);
  current.insert(current.end(), graph_manifest_sha256.begin(), graph_manifest_sha256.end());
  if (!write_sync(staging_root / "resource-graph-current", current) ||
      !replace_file(staging_root / "resource-graph-current", cache_root / "graph/current")) {
    detail = "cannot publish resource graph current pointer";
    return false;
  }
  detail.clear();
  return true;
}

bool RetailResourceGraph::open(const std::filesystem::path& cache_root,
                               const Sha256Digest& content_index_sha256) {
  valid_ = false; nodes_.clear(); detail_ = "resource graph is missing or incompatible";
  std::vector<std::uint8_t> current(kCurrentSize);
  std::ifstream pointer(cache_root / "graph/current", std::ios::binary);
  if (!pointer) return false;
  pointer.read(reinterpret_cast<char*>(current.data()), current.size());
  if (!pointer || !std::equal(kCurrentMagic.begin(), kCurrentMagic.end(), current.begin()) ||
      u32(static_cast<const std::vector<std::uint8_t>&>(current), 8) != kVersion ||
      u32(static_cast<const std::vector<std::uint8_t>&>(current), 12) != kCurrentSize) return false;
  std::copy_n(current.begin() + 16, manifest_sha256_.size(), manifest_sha256_.begin());
  const auto path = cache_root / "graph/manifests" /
      (sha256_hex(manifest_sha256_) + ".ac6graph");
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size > 64ull * 1024ull * 1024ull) return false;
  std::vector<std::uint8_t> manifest(static_cast<std::size_t>(size));
  std::ifstream input(path, std::ios::binary);
  input.read(reinterpret_cast<char*>(manifest.data()), manifest.size());
  if (!input || sha256_bytes(manifest) != manifest_sha256_ ||
      !parse_graph(manifest, content_index_sha256, nodes_, relations_)) return false;
  valid_ = true; detail_ = "resource graph is valid"; return true;
}

}  // namespace ac6
