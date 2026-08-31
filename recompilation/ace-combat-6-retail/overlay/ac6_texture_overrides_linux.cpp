#include <rex/cvar.h>

// The DDS replacement/export implementation is tied to D3D12 host types.
// Keep its public switches present on Linux, but make the feature unavailable.
REXCVAR_DEFINE_BOOL(ac6_texture_swaps_enabled, false, "AC6/TextureSwaps",
                    "Unavailable in the Linux/Vulkan retail product");
REXCVAR_DEFINE_BOOL(ac6_texture_swaps_dump_enabled, false, "AC6/TextureSwaps",
                    "Unavailable in the Linux/Vulkan retail product");
REXCVAR_DEFINE_BOOL(ac6_texture_swaps_replace_enabled, false, "AC6/TextureSwaps",
                    "Unavailable in the Linux/Vulkan retail product");

