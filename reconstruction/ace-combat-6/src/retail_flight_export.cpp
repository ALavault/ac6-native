// 0x82303898 and 0x82303940, instruction by instruction.

#include "ac6/retail_flight_export.h"

namespace ac6::retail {

FlightExportBlock export_flight_state_slot20(
    FlightExportBlock block, const FlightAccessorValues& values,
    const FlightExportFields& fields) noexcept {
  // Called 16, 17, 18; stored at +12, +8, +16. The two orders differ.
  block.at12 = values.slot16;
  block.at8 = values.slot17;
  block.at16 = values.slot18;

  block.at28 = fields.at376;
  block.at20 = fields.at368;
  block.at24 = fields.at372;
  block.at32 = fields.at32;
  block.at36 = fields.at356;
  block.at52 = fields.at376;      // the second copy, 0x82303920
  return block;
}

FlightExportBlock export_flight_state_slot21(
    FlightExportBlock block, const FlightAccessorValues& values,
    const FlightExportFields& fields) noexcept {
  // This one stores +376 FIRST, before any accessor runs.
  block.at44 = fields.at376;
  block.at48 = values.slot17;
  block.at52 = values.slot18;
  block.at56 = values.slot16;
  return block;
}

}  // namespace ac6::retail
