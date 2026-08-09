#pragma once

// A picture of the contracted flight model. NOT A PORT OF ANYTHING.
//
// EVERYTHING IN THIS FILE IS MINE, and that is the point of putting it in a file
// whose name starts with `demo_`. The campaign's rule is that no gameplay rule
// is invented; this file invents a camera and a scene *on purpose*, so that the
// rules that are not invented can be seen moving.
//
// WHAT IS RETAIL'S, MEASURED:
//   the attitude. Every rule that turns a stick position into the basis this
//   draws comes from retail_flight_session, which composes twenty-five
//   contracted behaviours, each verified bit-for-bit against the retail
//   instructions by micro-execution.
//
// WHAT IS MINE, INVENTED:
//   - the camera. Cycle 1396 established that retail's gameplay camera,
//     0x82300C20, uses vrefp and vrsqrtefp -- estimate instructions whose exact
//     bits belong to the console -- so a faithful one is not reachable by this
//     instrument and the campaign refuses to approximate it.
//   - the scene: a ground grid and a horizon. There is no retail geometry here
//     and none is claimed; loading it is the JV decision, still open.
//   - WHICH BASIS ROW IS WHICH AXIS. Nothing in the campaign has established
//     that row 0 is right, row 1 is up and row 2 is forward. This file assumes
//     it to have something to draw. If the assignment is wrong the picture is
//     wrong in a way the flight model is not, and that is a property of this
//     file alone.
//   - the field of view, the grid spacing, the altitude, the colours.
//
// So: a viewer sees retail's flight model through my camera, looking at my
// scene. Any caption that does not say all three is dishonest, and the
// `caption()` below exists so that it is not left to a caption to remember.

#include "ac6/retail_flight_step.h"
#include "ac6/retail_ndxr_geometry.h"
#include "ac6/retail_transform.h"

#include "ac6/retail_map_water.h"
#include "ac6/retail_terrain_field.h"

#include <cstdint>
#include <string>

#include <vector>

namespace ac6::demo {

struct Image {
  int width{};
  int height{};
  std::vector<std::uint8_t> rgb;   // width * height * 3
  // Depth, one float per pixel, larger is farther. Sized on the first
  // `clear_depth`; the wireframe drawing paths never touch it.
  std::vector<float> depth;

  void clear(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept;
  void clear_depth() noexcept;
  // A depth-tested, flat-shaded triangle in SCREEN space, with `z` the camera
  // depth at each corner. Does nothing when the depth buffer is unsized.
  void triangle(int x0, int y0, float z0, int x1, int y1, float z1,
                int x2, int y2, float z2,
                std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept;
  // The same, sampling `texels` (width x height, 0xAABBGGRR) at barycentric
  // texture coordinates and scaling by `shade`.
  //
  // AFFINE, NOT PERSPECTIVE-CORRECT. The interpolation is linear in screen
  // space, which warps a texture across a triangle seen at a steep angle. It is
  // wrong, it is cheap, and it is written down rather than discovered.
  void triangle_textured(int x0, int y0, float z0, float u0, float v0,
                         int x1, int y1, float z1, float u1, float v1,
                         int x2, int y2, float z2, float u2, float v2,
                         const std::uint32_t* texels, int tw, int th,
                         float shade) noexcept;
  void plot(int x, int y, std::uint8_t r, std::uint8_t g,
            std::uint8_t b) noexcept;
  void line(int x0, int y0, int x1, int y1, std::uint8_t r, std::uint8_t g,
            std::uint8_t b) noexcept;
  bool write_ppm(const char* path) const;
};

// Mine. The names say what they are.
struct DemoCamera {
  float invented_fov_y{1.0F};        // radians
  float invented_altitude{120.0F};   // metres above the invented grid
  float invented_grid_spacing{200.0F};
  int invented_grid_lines{21};
};

// Draws the grid and horizon as seen from an aircraft whose attitude is
// `basis` and whose position is `position`. Both come from retail arithmetic --
// the basis from the contracted rotation kernel, the position from the
// contracted integrator 0x82303110 -- and everything else here is not.
//
// THE GRID SNAPS TO THE EYE. Without that an aircraft that actually moves flies
// off a finite grid in about ten seconds. Quantising the grid origin to the
// spacing makes it read as unbounded; it is a rendering choice and it changes
// nothing about the position it is drawn from.
//
// The ground plane is world y = 0 and the eye sits at `position.at68` above it.
// `at68` is retail's vertical component -- the only one carrying the 10.0 floor
// and the only one the gravity bias touches (retail_flight_step.h) -- so the
// altitude in these pictures is integrated, not chosen. Which of at64/at72 is
// north and which is east remains unestablished and unclaimed.
void draw_flight_view(Image& image, const ac6::retail::RetailBasis& basis,
                      const DemoCamera& camera,
                      const ac6::retail::FlightPosition& position) noexcept;

// The stationary overload, kept because the still captures use it: the eye sits
// at `invented_altitude` over the origin and nothing moves.
void draw_flight_view(Image& image, const ac6::retail::RetailBasis& basis,
                      const DemoCamera& camera) noexcept;

// The same view, over the map's real ground instead of the invented grid.
//
// Every height comes from `TerrainField`, whose derivation is `0x82102568`, and
// every land/water decision from `MapWaterGrid`, whose derivation is
// `0x82101EE8`. The aircraft's attitude is the contracted rotation kernel and
// its position the contracted integrator, exactly as `draw_flight_view` uses
// them.
//
// WHAT IS STILL INVENTED, and it is the same list as the grid overload: the
// field of view, the colours, the light direction and the haze. And one more
// that matters here -- **which of `at64` and `at72` is north** is unestablished
// (see `draw_flight_view`), so this maps `at64` to world x and `at72` to world
// z because it must pick something. A wrong choice mirrors the map; it does not
// invent terrain.
//
// `water` may be null, in which case ground below `kSeaLevelForShading` is
// shaded as sea. That is a proxy and a poor one -- cycle 1445 measured the
// city's ground at exactly zero -- which is why the parameter exists.
inline constexpr float kSeaLevelForShading = 0.5F;
// The same, without clearing: the caller has already drawn a sky.
void draw_terrain_view_over(Image& image, const ac6::retail::RetailBasis& basis,
                            const DemoCamera& camera,
                            const ac6::retail::FlightPosition& position,
                            const ac6::retail::TerrainField& field,
                            const ac6::retail::MapWaterGrid* water) noexcept;

void draw_terrain_view(Image& image, const ac6::retail::RetailBasis& basis,
                       const DemoCamera& camera,
                       const ac6::retail::FlightPosition& position,
                       const ac6::retail::TerrainField& field,
                       const ac6::retail::MapWaterGrid* water) noexcept;

// Draws world-space triangles into the same view, depth-tested against whatever
// is already there -- so a caller draws the ground first and the buildings on
// top, and the depth buffer sorts them.
//
// `xyz` is nine floats per triangle, already in world coordinates. Flattening
// the parts is the caller's job because the placement, the model cache and the
// culling all belong to it; what belongs here is the projection, and it is the
// same `to_camera`/`project` pair everything else in this file uses.
//
// Each triangle is shaded by its own normal against a fixed light. That light is
// invented, like every other light in this file.
void draw_world_triangles(Image& image, const ac6::retail::RetailBasis& basis,
                          const DemoCamera& camera,
                          const ac6::retail::FlightPosition& position,
                          const std::vector<float>& xyz) noexcept;

// The same, textured: `uv` is six floats per triangle, `texels` is
// `tw * th` in 0xAABBGGRR, and `sun` is a unit direction the caller supplies.
//
// The sun is not invented here. Mission 01's mapset carries `.sky1.sun.lrx` and
// `.sky1.sun.lry` -- 40 and 145 degrees -- and `.sky1.fog.far` / `.density`;
// `fog_far` and `fog_density` are those, and the fog colour is the caller's sky.
// Cycle 1474 read them out of the archive's own XML, which is why this signature
// has parameters where the earlier one had constants.
void draw_world_triangles_textured(Image& image,
                                   const ac6::retail::RetailBasis& basis,
                                   const DemoCamera& camera,
                                   const ac6::retail::FlightPosition& position,
                                   const std::vector<float>& xyz,
                                   const std::vector<float>& uv,
                                   const std::uint32_t* texels, int tw, int th,
                                   const float sun[3], float fog_far,
                                   float fog_density,
                                   std::uint8_t fog_r, std::uint8_t fog_g,
                                   std::uint8_t fog_b) noexcept;

// Draws a decoded NDXR mesh as a wireframe, seen from `basis` at `position`.
//
// THE MESH IS RETAIL'S AND THE REST OF THE PICTURE IS NOT. Every vertex comes
// through contracted resolution -- ModelDirectory, ContainerIndex,
// NdxrContainer -- and the strip is walked with retail's own 0xFFFF restart.
// The camera, the framing and the colours are this file's.
//
// STRIPS ARE DRAWN AS EDGES, not filled. There is no material, no texture and
// no winding rule here, and drawing triangles would imply all three; the
// wireframe claims only the positions and the connectivity, which are what
// cycle 1426 established.
void draw_mesh_wireframe(Image& image, const ac6::retail::NdxrMesh& mesh,
                         const ac6::retail::RetailBasis& basis,
                         const DemoCamera& camera, float distance) noexcept;

// Draws a mesh at a WORLD OFFSET from the eye, rather than centred in frame.
//
// `draw_mesh_wireframe` above frames one model against its own bounds, which is
// right for a contact sheet and wrong for a scene: it would put every unit in
// the middle. This takes the offset from the eye to the model's origin, in the
// same axes the scenario's positions use, and leaves the framing to the caller.
//
// `image` is NOT cleared here -- a scene is many of these.
void draw_mesh_at(Image& image, const ac6::retail::NdxrMesh& mesh,
                  const ac6::retail::RetailBasis& basis, const DemoCamera& camera,
                  float ox, float oy, float oz,
                  std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept;

// The mesh as filled, depth-tested triangles, flat-shaded by the face's own
// vertex normals.
//
// NO BACKFACE CULLING, and that is a claim this cannot make: no winding rule has
// been read out of retail, so discarding a face by its orientation would be
// inventing one. Both sides are drawn and the shade uses the absolute value of
// the light dot, which is what a two-sided surface looks like.
//
// The strip is walked with retail's 0xFFFF restart, and the winding alternates
// every triangle as a strip's does -- that part is the format's, not a choice.
void draw_mesh_solid(Image& image, const ac6::retail::NdxrMesh& mesh,
                     const ac6::retail::RetailBasis& basis, const DemoCamera& camera,
                     float distance) noexcept;

// The mesh as textured, depth-tested triangles.
//
// `texels` is a decoded NTXR base level -- 0xAABBGGRR, row-major. The texture
// coordinates are the vertex ones, decoded at cycle 1433 and unused until now.
// Shading is the same flat two-sided term as `draw_mesh_solid`, multiplied in.
//
// WRAPPING IS `repeat`, chosen: 2.7% of the package's coordinates fall outside
// [0,1] and nothing has been read that says how retail wraps them.
void draw_mesh_textured(Image& image, const ac6::retail::NdxrMesh& mesh,
                        const ac6::retail::RetailBasis& basis,
                        const DemoCamera& camera, float distance,
                        const std::uint32_t* texels, int tw, int th) noexcept;

// The same wireframe, with each segment lit by the vertex normal.
//
// The normals are retail's -- four float16 per vertex at +12, unit-length for
// 178,973 of the package's 179,322 and exactly zero for the other 349. The
// LIGHT DIRECTION is chosen, and so is the ramp from a dot product to a colour.
// This is not shading a surface: there is no depth buffer and no fill, so a
// far edge draws over a near one. It shows that the normals are real, which a
// flat wireframe cannot.
void draw_mesh_lit(Image& image, const ac6::retail::NdxrMesh& mesh,
                   const ac6::retail::RetailBasis& basis, const DemoCamera& camera,
                   float distance) noexcept;

// The sentence that must accompany any picture this file produces. It is a
// function rather than a comment so that a caller cannot forget it and a test
// can assert it is present.
std::string caption() noexcept;

}  // namespace ac6::demo
