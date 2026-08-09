#include "ac6/retail_flight_export.h"

#include <cmath>
#include <cstdio>

namespace {
int failures = 0;
void check(bool c, const char* w) { if (!c) { std::printf("FAIL  %s\n", w); ++failures; } }
void check_bits(float a, float b, const char* w) {
  if (std::signbit(a) != std::signbit(b) || !(a == b)) {
    std::printf("FAIL  %s  (got %.9g, want %.9g)\n", w, a, b); ++failures;
  }
}
using ac6::retail::export_flight_state_slot20;
using ac6::retail::export_flight_state_slot21;
using ac6::retail::FlightAccessorValues;
using ac6::retail::FlightExportBlock;
using ac6::retail::FlightExportFields;

FlightAccessorValues values() { return FlightAccessorValues{1.0F, 2.0F, 3.0F}; }
FlightExportFields fields() {
  return FlightExportFields{10.0F, 20.0F, 30.0F, 40.0F, 50.0F};
}
FlightExportBlock seeded() {
  FlightExportBlock b{};
  b.at8 = b.at12 = b.at16 = b.at20 = b.at24 = b.at28 = -1.0F;
  b.at32 = b.at36 = b.at44 = b.at48 = b.at52 = b.at56 = -1.0F;
  return b;
}

void slot20_stores_the_accessors_in_a_different_order_than_it_calls_them() {
  const FlightExportBlock out = export_flight_state_slot20(seeded(), values(),
                                                           fields());
  check_bits(out.at12, 1.0F, "slot 16 goes to +12");
  check_bits(out.at8, 2.0F, "slot 17 goes to +8");
  check_bits(out.at16, 3.0F, "slot 18 goes to +16");
  check(out.at8 != out.at12, "the two orders really do differ");
}

void slot20_copies_the_raw_fields() {
  const FlightExportBlock out = export_flight_state_slot20(seeded(), values(),
                                                           fields());
  check_bits(out.at20, 30.0F, "+368 -> +20");
  check_bits(out.at24, 40.0F, "+372 -> +24");
  check_bits(out.at28, 50.0F, "+376 -> +28");
  check_bits(out.at32, 10.0F, "+32 -> +32");
  check_bits(out.at36, 20.0F, "+356 -> +36");
}

void slot20_writes_376_twice() {
  const FlightExportBlock out = export_flight_state_slot20(seeded(), values(),
                                                           fields());
  check_bits(out.at28, 50.0F, "+376 lands at +28");
  check_bits(out.at52, 50.0F, "and again at +52");
}

void slot20_leaves_the_words_it_does_not_write() {
  const FlightExportBlock out = export_flight_state_slot20(seeded(), values(),
                                                           fields());
  check_bits(out.at44, -1.0F, "+44 is untouched by slot 20");
  check_bits(out.at48, -1.0F, "+48 is untouched");
  check_bits(out.at56, -1.0F, "+56 is untouched");
}

void slot21_writes_four_words_and_no_others() {
  const FlightExportBlock out = export_flight_state_slot21(seeded(), values(),
                                                           fields());
  check_bits(out.at44, 50.0F, "+376 -> +44");
  check_bits(out.at48, 2.0F, "slot 17 -> +48");
  check_bits(out.at52, 3.0F, "slot 18 -> +52");
  check_bits(out.at56, 1.0F, "slot 16 -> +56");
  check_bits(out.at8, -1.0F, "+8 is untouched by slot 21");
  check_bits(out.at28, -1.0F, "+28 is untouched");
}

void the_two_exporters_disagree_about_plus_52() {
  // Slot 20 puts +376 there; slot 21 puts the blended slot 18 there. Running
  // both in either order leaves whichever ran last, and that is worth pinning
  // because the overlap is the one place their outputs could be confused.
  const FlightExportBlock twenty =
      export_flight_state_slot20(seeded(), values(), fields());
  const FlightExportBlock twentyone =
      export_flight_state_slot21(seeded(), values(), fields());
  check(twenty.at52 != twentyone.at52,
        "+52 is +376 from slot 20 and slot 18 from slot 21");
}

void the_controls_all_bite() {
  int same_order = 0, single_376 = 0;
  for (int i = 0; i < 16; ++i) {
    FlightAccessorValues v{static_cast<float>(i), static_cast<float>(i) + 1.0F,
                           static_cast<float>(i) + 2.0F};
    FlightExportFields f = fields();
    f.at376 = static_cast<float>(i) * 3.0F;
    const FlightExportBlock out = export_flight_state_slot20(seeded(), v, f);
    if (out.at8 != v.slot16) { ++same_order; }
    if (out.at52 != out.at28) { ++single_376; }
  }
  std::printf("controls: call-order-is-store-order=%d one-376-copy=%d\n",
              same_order, single_376);
  check(same_order > 0,
        "CONTROL assuming call order is store order must disagree");
  check(single_376 == 0, "CONTROL both +376 copies must match");
}
}  // namespace

int main() {
  slot20_stores_the_accessors_in_a_different_order_than_it_calls_them();
  slot20_copies_the_raw_fields();
  slot20_writes_376_twice();
  slot20_leaves_the_words_it_does_not_write();
  slot21_writes_four_words_and_no_others();
  the_two_exporters_disagree_about_plus_52();
  the_controls_all_bite();
  if (failures != 0) {
    std::printf("retail_flight_export: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("retail_flight_export: all cases passed\n");
  return 0;
}
