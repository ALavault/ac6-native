// The transform kernel against thirteen micro-executions of retail.
//
// TWO TOLERANCES, AND THE DIFFERENCE BETWEEN THEM IS THE POINT.
//
// The rotation arithmetic is exact and is compared exactly: `rotate_basis` takes
// the cosine/sine pair, so a test that supplies retail's own pair must match to
// the bit. Every zero-angle case does exactly that, because cos(0) and sin(0) are
// representable and identical everywhere.
//
// The angle-taking wrappers go through `retail_sin_cos`, which is the host
// library standing in for 0x8209CB70. That is the ONLY reason a tolerance
// appears, and it is applied only to the cases that use it.

#include "ac6/retail_transform.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

float from_bits(std::uint32_t bits) {
  float value = 0.0F;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

std::uint32_t to_bits(float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

std::vector<std::string> split(const std::string& text, char separator) {
  std::vector<std::string> parts;
  std::stringstream stream(text);
  std::string item;
  while (std::getline(stream, item, separator)) {
    parts.push_back(item);
  }
  return parts;
}

// The seam's error budget. Measured, not chosen: cycle 1327's model check over
// the same twelve rotation cases had a worst deviation of 1.5e-06 against exact
// double-precision trigonometry, so anything near that is the trigonometry and
// anything far above it is the arithmetic.
constexpr float kTrigTolerance = 2e-5F;

void test_identity_is_read_not_assumed() {
  const ac6::retail::RetailBasis basis = ac6::retail::identity_basis();
  check(to_bits(basis.rows[0][0]) == 0x3F800000U, "identity row0 lane0 is 1.0");
  check(to_bits(basis.rows[1][1]) == 0x3F800000U, "identity row1 lane1 is 1.0");
  check(to_bits(basis.rows[2][2]) == 0x3F800000U, "identity row2 lane2 is 1.0");
  check(to_bits(basis.rows[0][3]) == 0U, "identity row0 lane3 is +0");
}

// The arithmetic, with the trigonometry taken out of the question entirely.
void test_rotation_is_exact_given_the_pair() {
  ac6::retail::RetailBasis basis;
  basis.rows[0] = {1.0F, 2.0F, 3.0F, 4.0F};
  basis.rows[1] = {5.0F, 6.0F, 7.0F, 8.0F};
  basis.rows[2] = {9.0F, 10.0F, 11.0F, 12.0F};
  const ac6::retail::RetailBasis before = basis;

  // A pair that is not a real cosine/sine, so the test cannot pass by accident
  // of trigonometric identity: it checks the update rule and nothing else.
  ac6::retail::rotate_basis(basis, 1, -1, ac6::retail::SinCos{2.0F, 3.0F});
  check(basis.rows[1] == before.rows[1], "the kept row is untouched");
  // row0' = row0*3 - 2*row2 ; row2' = 2*row0 + row2*3
  check(to_bits(basis.rows[0][0]) == to_bits(1.0F * 3.0F - 2.0F * 9.0F),
        "moving row 0 is exact");
  check(to_bits(basis.rows[2][0]) == to_bits(2.0F * 1.0F + 9.0F * 3.0F),
        "moving row 2 is exact");

  // The aliasing hazard: both sources must be read before either is written.
  // With the naive in-place update the second row would use the already-written
  // first, and this catches exactly that.
  check(to_bits(basis.rows[2][1]) == to_bits(2.0F * 2.0F + 10.0F * 3.0F),
        "the second moving row used the ORIGINAL first row");
}

int run_differential(const char* path) {
  std::ifstream file(path);
  if (!file) {
    std::cerr << "cannot open " << path << "\n";
    return -1;
  }
  std::string line;
  int compared = 0;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    const std::vector<std::string> columns = split(line, '\t');
    if (columns.size() != 16) {
      std::cerr << "FAIL malformed row: " << columns[0] << "\n";
      ++failures;
      continue;
    }
    const std::string& name = columns[0];
    const std::string& kind = columns[1];

    float angle1 = 0.0F;
    float angle2 = 0.0F;
    float angle3 = 0.0F;
    for (const std::string& item : split(columns[3], ',')) {
      const std::size_t equals = item.find('=');
      const float value = std::stof(item.substr(equals + 1));
      const std::string which = item.substr(0, equals);
      if (which == "f1") {
        angle1 = value;
      } else if (which == "f2") {
        angle2 = value;
      } else if (which == "f3") {
        angle3 = value;
      }
    }

    ac6::retail::RetailBasis basis;
    bool zero_angle = false;
    if (kind == "assemble") {
      basis = ac6::retail::assemble_basis(angle1, angle2, angle3);
      zero_angle = (angle1 == 0.0F && angle2 == 0.0F && angle3 == 0.0F);
    } else {
      basis.rows[0] = {1.0F, 2.0F, 3.0F, 4.0F};
      basis.rows[1] = {5.0F, 6.0F, 7.0F, 8.0F};
      basis.rows[2] = {9.0F, 10.0F, 11.0F, 12.0F};
      const std::string address = kind.substr(kind.find(':') + 1);
      if (address == "0x820A9B30") {
        ac6::retail::rotate_820A9B30(basis, angle1);
      } else if (address == "0x820A99F8") {
        ac6::retail::rotate_820A99F8(basis, angle1);
      } else if (address == "0x82211828") {
        ac6::retail::rotate_82211828(basis, angle1);
      } else {
        std::cerr << "FAIL unknown kind " << kind << " in " << name << "\n";
        ++failures;
        continue;
      }
      zero_angle = (angle1 == 0.0F);
    }

    for (int row = 0; row < 3; ++row) {
      for (int lane = 0; lane < 4; ++lane) {
        const std::uint32_t want = static_cast<std::uint32_t>(
            std::stoul(columns[4 + row * 4 + lane], nullptr, 16));
        const float got = basis.rows[static_cast<std::size_t>(row)]
                                   [static_cast<std::size_t>(lane)];
        // A zero-angle case goes through no approximation on either side, so it
        // is required to match BIT FOR BIT. Only the turned cases get the seam's
        // tolerance.
        if (zero_angle) {
          if (to_bits(got) != want) {
            std::cerr << "FAIL " << name << " row" << row << " lane" << lane
                      << ": got 0x" << std::hex << to_bits(got) << " want 0x"
                      << want << std::dec << " (zero angle, exact required)\n";
            ++failures;
          }
          continue;
        }
        const float expected = from_bits(want);
        if (std::fabs(got - expected) > kTrigTolerance) {
          std::cerr << "FAIL " << name << " row" << row << " lane" << lane
                    << ": got " << got << " want " << expected << "\n";
          ++failures;
        }
      }
    }
    ++compared;
  }
  std::cout << "differential vectors=" << compared << "\n";
  if (compared == 0) {
    std::cerr << "FAIL the vector table produced no comparisons\n";
    ++failures;
  }
  return compared;
}

}  // namespace

int main(int argc, char** argv) {
  test_identity_is_read_not_assumed();
  test_rotation_is_exact_given_the_pair();
  int compared = 0;
  if (argc > 1) {
    compared = run_differential(argv[1]);
    if (compared < 0) {
      return 1;
    }
  } else {
    std::cerr << "FAIL no vector table given; the differential did not run\n";
    ++failures;
  }
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  if (argc > 2) {
    std::ofstream report(argv[2]);
    report << "{\n"
           << "  \"schema\": \"ac6.retail-transform-test.v1\",\n"
           << "  \"vectors\": " << compared << ",\n"
           << "  \"divergences\": 0,\n"
           << "  \"zero_angle_cases\": \"required bit-exact\",\n"
           << "  \"turned_case_tolerance\": 2e-5,\n"
           << "  \"statement\": \"every micro-executed vector of 0x822A1E80 and "
              "its three rotations reproduced; zero-angle cases bit for bit, "
              "turned cases within a tolerance that exists only because "
              "0x8209CB70 (XMScalarSinCos) is not ported and the host library "
              "stands in for it\"\n"
           << "}\n";
  }
  std::cout << "retail_transform=pass\n";
  return 0;
}
