#pragma once

// The initial Mission 01 product path uses the already-qualified, hand-written
// SPIR-V fixtures while the retail shader census is being closed.  Keeping this
// adapter in src (rather than the installed API) makes the dependency explicit
// and prevents generated/oracle shader output from entering the product.
#include "../tests/fixtures/vulkan_clip_mesh_spirv.h"
#include "../tests/fixtures/vulkan_textured_triangle_spirv.h"
#include "../tests/fixtures/vulkan_world_textured_spirv.h"

#include <array>
#include <cstdint>

namespace ac6::retail_cli::detail {

inline constexpr const auto& kRetailClipVertexSpirv =
    ac6_test::kClipMeshVertexSpirv;
inline constexpr const auto& kRetailTexturedFragmentSpirv =
    ac6_test::kTexturedTriangleFragmentSpirv;
inline constexpr const auto& kRetailWorldVertexSpirv =
    ac6_test::kWorldTexturedVertexSpirv;
inline constexpr const auto& kRetailWorldFragmentSpirv =
    ac6_test::kTexturedTriangleFragmentSpirv;

}  // namespace ac6::retail_cli::detail
