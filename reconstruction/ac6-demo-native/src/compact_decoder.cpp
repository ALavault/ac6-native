#include "ac6demo_native/compact_decoder.hpp"

#include <bit>

namespace ac6demo_native {
namespace {

class BitReader {
public:
    explicit BitReader(std::span<const std::byte> input) : input_(input) {}

    [[nodiscard]] bool read(unsigned count, std::uint32_t* value) noexcept {
        if (count > 16U) {
            return false;
        }
        if (count == 0U) {
            *value = 0U;
            return true;
        }
        while (bits_ < count) {
            if (cursor_ > input_.size() || input_.size() - cursor_ < 2U) {
                return false;
            }
            const auto low = std::to_integer<std::uint16_t>(input_[cursor_]);
            const auto high = std::to_integer<std::uint16_t>(input_[cursor_ + 1U]);
            const auto word = static_cast<std::uint16_t>(low | (high << 8U));
            reservoir_ |= static_cast<std::uint64_t>(word) << bits_;
            cursor_ += 2U;
            bits_ += 16U;
        }
        const auto mask = (std::uint64_t{1} << count) - 1U;
        *value = static_cast<std::uint32_t>(reservoir_ & mask);
        reservoir_ >>= count;
        bits_ -= count;
        return true;
    }

    [[nodiscard]] std::size_t cursor() const noexcept { return cursor_; }

private:
    std::span<const std::byte> input_;
    std::size_t cursor_ = 0;
    std::uint64_t reservoir_ = 0;
    unsigned bits_ = 0;
};

[[nodiscard]] bool valid_table(const CompactTables& tables) noexcept {
    if (tables.primary_width > 16U || tables.distance_width > 16U) {
        return false;
    }
    const auto primary_count = std::size_t{1} << tables.primary_width;
    const auto distance_count = std::size_t{1} << tables.distance_width;
    return tables.primary.size() >= primary_count &&
           tables.distance.size() >= distance_count;
}

[[nodiscard]] bool read_extra(BitReader* reader, const CompactNode& node,
                              std::uint32_t* value) noexcept {
    return node.extra_bits <= 16U && reader->read(node.extra_bits, value);
}

}  // namespace

CompactDecodeResult decode_compact(std::span<const std::byte> input,
                                   std::span<std::byte> output,
                                   CompactTables tables) noexcept {
    CompactDecodeResult result;
    BitReader reader(input);
    if (!valid_table(tables)) {
        result.status = CompactDecodeStatus::invalid_table;
        return result;
    }

    std::size_t output_size = 0;
    const std::size_t transition_limit = tables.primary.size() + 1U;
    while (true) {
        std::uint32_t index = 0U;
        if (!reader.read(tables.primary_width, &index)) {
            result.status = CompactDecodeStatus::truncated_input;
            break;
        }

        std::size_t transitions = 0;
        while (true) {
            if (index >= tables.primary.size() || transitions++ > transition_limit) {
                result.status = CompactDecodeStatus::invalid_table;
                goto done;
            }
            const CompactNode& node = tables.primary[index];
            std::uint32_t ignored = 0U;
            if (node.bits > 16U || !reader.read(node.bits, &ignored)) {
                result.status = CompactDecodeStatus::truncated_input;
                goto done;
            }

            switch (node.type) {
            case 1: {
                if (output_size == output.size()) {
                    result.status = CompactDecodeStatus::output_overflow;
                    goto done;
                }
                output[output_size++] =
                    std::byte{static_cast<unsigned char>(node.base & 0xffU)};
                goto next_symbol;
            }
            case 2: {
                std::uint32_t extra_length = 0U;
                if (!read_extra(&reader, node, &extra_length)) {
                    result.status = CompactDecodeStatus::truncated_input;
                    goto done;
                }
                const auto length = static_cast<std::size_t>(node.base) + extra_length;

                std::uint32_t distance_index = 0U;
                if (!reader.read(tables.distance_width, &distance_index) ||
                    distance_index >= tables.distance.size()) {
                    result.status = CompactDecodeStatus::truncated_input;
                    goto done;
                }
                const CompactNode& distance_node = tables.distance[distance_index];
                if (distance_node.type != 2U || distance_node.bits > 16U) {
                    result.status = CompactDecodeStatus::invalid_table;
                    goto done;
                }
                if (!reader.read(distance_node.bits, &ignored)) {
                    result.status = CompactDecodeStatus::truncated_input;
                    goto done;
                }
                std::uint32_t extra_distance = 0U;
                if (!read_extra(&reader, distance_node, &extra_distance)) {
                    result.status = CompactDecodeStatus::truncated_input;
                    goto done;
                }
                const auto distance = static_cast<std::size_t>(distance_node.base) +
                                      extra_distance;
                if (distance == 0U || distance > output_size ||
                    length > output.size() - output_size) {
                    result.status = CompactDecodeStatus::output_overflow;
                    goto done;
                }
                for (std::size_t i = 0; i < length; ++i) {
                    output[output_size] = output[output_size - distance];
                    ++output_size;
                }
                goto next_symbol;
            }
            case 3:
                result.status = CompactDecodeStatus::complete;
                goto done;
            case 4: {
                std::uint32_t extra = 0U;
                if (!read_extra(&reader, node, &extra)) {
                    result.status = CompactDecodeStatus::truncated_input;
                    goto done;
                }
                index = static_cast<std::uint32_t>(node.base) + extra;
                continue;
            }
            default:
                result.status = CompactDecodeStatus::invalid_table;
                goto done;
            }
        }

    next_symbol:
        continue;
    }

done:
    result.input_consumed = reader.cursor();
    result.output_size = output_size;
    return result;
}

CompactXorStatus xor_repeating_be64(std::span<std::byte> output,
                                    std::uint64_t key, std::size_t cursor,
                                    std::size_t limit) noexcept {
    if (cursor > limit || limit > output.size()) {
        return CompactXorStatus::output_overflow;
    }

    for (std::size_t index = cursor; index < limit; ++index) {
        const auto key_shift = static_cast<unsigned>((index - cursor) % 8U);
        const auto shift = 56U - (key_shift * 8U);
        const auto key_byte = static_cast<unsigned char>((key >> shift) & 0xffU);
        const auto value = std::to_integer<unsigned char>(output[index]);
        output[index] = std::byte{static_cast<unsigned char>(value ^ key_byte)};
    }
    return CompactXorStatus::complete;
}

}  // namespace ac6demo_native
