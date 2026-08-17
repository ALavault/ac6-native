#pragma once

#include <cstdint>
#include <limits>

// Saturating narrowing and proleptic-Gregorian civil date conversion used
// by the audio-memory and graphics MMIO dispatch fragments.  Included from
// guest_bridge.cpp inside its anonymous namespace.
[[nodiscard]] std::int16_t saturate_s16(std::int32_t value) noexcept {
  if (value < std::numeric_limits<std::int16_t>::min()) {
    return std::numeric_limits<std::int16_t>::min();
  }
  if (value > std::numeric_limits<std::int16_t>::max()) {
    return std::numeric_limits<std::int16_t>::max();
  }
  return static_cast<std::int16_t>(value);
}
struct CivilDate final {
  std::int32_t year{};
  std::uint16_t month{};
  std::uint16_t day{};
};
[[nodiscard]] CivilDate civil_from_unix_days(std::int64_t days) noexcept {
  days += 719468;
  const auto era = (days >= 0 ? days : days - 146096) / 146097;
  const auto day_of_era = static_cast<std::uint32_t>(days - era * 146097);
  const auto year_of_era = (day_of_era - day_of_era / 1460U +
                            day_of_era / 36524U - day_of_era / 146096U) /
                           365U;
  auto year = static_cast<std::int32_t>(year_of_era) +
              static_cast<std::int32_t>(era) * 400;
  const auto day_of_year =
      day_of_era - (365U * year_of_era + year_of_era / 4U - year_of_era / 100U);
  const auto month_part = (5U * day_of_year + 2U) / 153U;
  const auto day = day_of_year - (153U * month_part + 2U) / 5U + 1U;
  const auto month =
      static_cast<std::int32_t>(month_part) + (month_part < 10U ? 3 : -9);
  year += month <= 2 ? 1 : 0;
  return CivilDate{year, static_cast<std::uint16_t>(month),
                   static_cast<std::uint16_t>(day)};
}
