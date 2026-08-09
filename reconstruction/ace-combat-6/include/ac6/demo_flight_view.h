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

#include <cstdint>
#include <string>
#include <vector>

namespace ac6::demo {

struct Image {
  int width{};
  int height{};
  std::vector<std::uint8_t> rgb;   // width * height * 3

  void clear(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept;
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

// The sentence that must accompany any picture this file produces. It is a
// function rather than a comment so that a caller cannot forget it and a test
// can assert it is present.
std::string caption() noexcept;

}  // namespace ac6::demo
