#include "ac6/retail_mission01_map_render_assets.h"

#include "ac6/retail_ndxr_container.h"

#include <algorithm>
#include <array>
#include <set>
#include <utility>

namespace ac6::retail {
namespace {

constexpr std::size_t kMapModelCount = 170;
constexpr std::size_t kMapRecordCount = 4318;
constexpr std::size_t kAcceptedInstanceCount = 4226;
constexpr std::size_t kSkippedInstanceCount = 92;

bool same_class(Mission01MapDrawClass left, std::uint8_t right) noexcept {
  return static_cast<std::uint8_t>(left) == right;
}

}  // namespace

std::optional<Mission01MapDrawClass> mission01_map_draw_class(
    std::string_view record_name) noexcept {
  constexpr std::string_view marker = "_m01_";
  const std::size_t marker_at = record_name.find(marker);
  if (marker_at == std::string_view::npos) return std::nullopt;
  const std::size_t token_at = marker_at + marker.size();
  const std::size_t token_end = record_name.find('_', token_at);
  if (token_end == std::string_view::npos || token_end == token_at) {
    return std::nullopt;
  }
  const std::string_view token =
      record_name.substr(token_at, token_end - token_at);
  if (token == "l" || token == "airport") {
    return Mission01MapDrawClass::Large;
  }
  if (token == "m") return Mission01MapDrawClass::Medium;
  if (token == "s") return Mission01MapDrawClass::Small;
  if (token == "x") return Mission01MapDrawClass::Extra;
  return std::nullopt;
}

RetailMission01MapRenderAssets::RetailMission01MapRenderAssets(
    RetailMission01SceneBundle scene)
    : scene_(std::move(scene)) {}

std::optional<RetailMission01MapRenderAssets>
RetailMission01MapRenderAssets::open(const RetailContentStore& store) {
  std::optional<RetailMission01SceneBundle> scene =
      RetailMission01SceneBundle::open(store);
  if (!scene.has_value() || !scene->store_backed()) return std::nullopt;
  return build(std::move(*scene));
}

std::optional<RetailMission01MapRenderAssets>
RetailMission01MapRenderAssets::build_for_testing(
    RetailMission01SceneBundle scene) {
  return build(std::move(scene));
}

std::optional<RetailMission01MapRenderAssets>
RetailMission01MapRenderAssets::build(RetailMission01SceneBundle scene) {
  RetailMission01MapRenderAssets assets(std::move(scene));
  if (!assets.load_models() || !assets.load_textures() ||
      !assets.bind_instances()) {
    return std::nullopt;
  }
  Mission01MapRenderAssetReport& report = assets.report_;
  report.complete = report.model_files == kMapModelCount &&
                    report.source_records == kMapRecordCount &&
                    report.decoded_primitives == kMapRecordCount &&
                    report.texture_references == kMapRecordCount &&
                    report.texture_assets == kMapModelCount &&
                    report.source_instances == kMapRecordCount &&
                    report.record_bindings == kMapRecordCount &&
                    report.draw_instances == kAcceptedInstanceCount &&
                    report.skipped_instances == kSkippedInstanceCount;
  return report.complete
             ? std::optional<RetailMission01MapRenderAssets>(std::move(assets))
             : std::nullopt;
}

bool RetailMission01MapRenderAssets::load_models() {
  const std::optional<RetailFhmView> parts = scene_.map_parts();
  if (!parts.has_value() || parts->child_count() < kMapModelCount) return false;
  models_.reserve(kMapModelCount);
  for (std::uint16_t selector = 0; selector < kMapModelCount; ++selector) {
    const std::optional<std::span<const std::uint8_t>> bytes =
        parts->child(selector);
    if (!bytes.has_value() || !load_model(selector, *bytes)) return false;
  }
  return true;
}

bool RetailMission01MapRenderAssets::load_model(
    std::uint16_t selector, std::span<const std::uint8_t> bytes) {
  const std::optional<NdxrContainer> container =
      NdxrContainer::Open(bytes.data(), bytes.size());
  if (!container.has_value()) return false;

  Mission01MapModel model;
  model.selector = selector;
  model.records.reserve(container->record_count());
  for (std::uint16_t index = 0; index < container->record_count(); ++index) {
    const std::optional<NdxrRecord> record = container->Record(index);
    if (!record.has_value() || record->index != index ||
        record->descriptor_count != 1) {
      return false;
    }
    const std::optional<Mission01MapDrawClass> draw_class =
        mission01_map_draw_class(record->name);
    const std::optional<NdxrDescriptor> descriptor =
        container->Descriptor(*record, 0);
    const std::optional<NdxrMaterial> material =
        container->Material(*record, 0, 0);
    if (!draw_class.has_value() || !descriptor.has_value() ||
        !material.has_value() || material->texture_count != 1) {
      return false;
    }
    for (unsigned slot = 1; slot < NdxrContainer::kMaterialSlots; ++slot) {
      if (container->Material(*record, 0, slot).has_value()) return false;
    }
    const std::optional<NdxrTextureRef> texture =
        container->TextureRef(*material, 0);
    std::optional<NdxrMesh> geometry = decode_ndxr_descriptor(
        *container, bytes.data(), bytes.size(), *descriptor);
    if (!texture.has_value() || texture->texture_id == 0 ||
        !geometry.has_value() ||
        geometry->normals.size() != geometry->positions.size() ||
        geometry->texcoords.size() != geometry->positions.size()) {
      return false;
    }

    report_.vertices += geometry->positions.size();
    report_.indices += geometry->indices.size();
    ++report_.source_records;
    ++report_.decoded_primitives;
    ++report_.texture_references;
    model.records.push_back({std::string(record->name), *draw_class,
                             texture->texture_id, std::move(*geometry)});
  }
  ++report_.model_files;
  models_.push_back(std::move(model));
  return true;
}

bool RetailMission01MapRenderAssets::load_textures() {
  std::set<std::uint32_t> identifiers;
  for (const Mission01MapModel& model : models_) {
    for (const Mission01MapPrimitive& record : model.records) {
      identifiers.insert(record.texture_identifier);
    }
  }
  if (identifiers.size() != kMapModelCount) return false;

  const std::optional<std::vector<Mission01TextureResourceView>> resources =
      scene_.texture_resources();
  if (!resources.has_value()) return false;
  textures_.reserve(identifiers.size());
  for (const Mission01TextureResourceView& resource : *resources) {
    if (!identifiers.contains(resource.identifier)) continue;
    const std::optional<NtxrDescriptor> descriptor = parse_ntxr_descriptor(
        resource.bytes.data(), resource.bytes.size());
    if (!descriptor.has_value()) return false;
    textures_.push_back({resource.identifier, *descriptor, resource.bytes});
  }
  if (textures_.size() != identifiers.size()) return false;
  report_.texture_assets = textures_.size();
  return true;
}

bool RetailMission01MapRenderAssets::bind_instances() {
  std::vector<std::vector<bool>> referenced;
  referenced.reserve(models_.size());
  for (const Mission01MapModel& model : models_) {
    referenced.emplace_back(model.records.size(), false);
  }

  const std::vector<MapInstance>& source = scene_.placement().instances();
  report_.source_instances = source.size();
  draw_instances_.reserve(source.size());
  for (const MapInstance& instance : source) {
    if (instance.selector >= models_.size()) return false;
    const Mission01MapModel& model = models_[instance.selector];
    if (instance.record_index >= model.records.size() ||
        referenced[instance.selector][instance.record_index]) {
      return false;
    }
    const Mission01MapPrimitive& primitive =
        model.records[instance.record_index];
    if (!same_class(primitive.draw_class, instance.draw_class)) return false;
    referenced[instance.selector][instance.record_index] = true;
    ++report_.record_bindings;
    if (!instance.accepted) {
      ++report_.skipped_instances;
      continue;
    }
    ++report_.draw_classes[instance.draw_class];
    draw_instances_.push_back(
        {instance.world_x, instance.world_y, instance.world_z,
         instance.selector, instance.record_index, primitive.draw_class});
  }
  for (const std::vector<bool>& model : referenced) {
    if (std::find(model.begin(), model.end(), false) != model.end()) return false;
  }
  report_.draw_instances = draw_instances_.size();
  return true;
}

const Mission01MapPrimitive* RetailMission01MapRenderAssets::primitive_for(
    const Mission01MapDrawInstance& instance) const noexcept {
  if (instance.selector >= models_.size()) return nullptr;
  const Mission01MapModel& model = models_[instance.selector];
  if (instance.record_index >= model.records.size()) return nullptr;
  const Mission01MapPrimitive& primitive = model.records[instance.record_index];
  return primitive.draw_class == instance.draw_class ? &primitive : nullptr;
}

std::optional<DecodedTexture> RetailMission01MapRenderAssets::decode_texture(
    std::uint32_t identifier, bool swap_16, NtxrRefusal* refusal) const noexcept {
  const auto found = std::lower_bound(
      textures_.begin(), textures_.end(), identifier,
      [](const Mission01MapTextureAsset& asset, std::uint32_t value) {
        return asset.identifier < value;
      });
  if (found == textures_.end() || found->identifier != identifier) {
    if (refusal != nullptr) *refusal = NtxrRefusal::BadHeader;
    return std::nullopt;
  }
  return decode_ntxr_base_level(found->source.data(), found->source.size(),
                                swap_16, refusal);
}

}  // namespace ac6::retail
