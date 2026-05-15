#pragma once

#include <string>
#include <vector>
#include <span>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>

#include "ProtoUtils.h"
#include "FlatProtoPath.h"

namespace proto
{
    struct RawWireField
    {
        int      field_number    = 0;
        int      wire_type       = -1;
        uint64_t varint_or_bits  = 0;
        std::span<const std::byte> len_bytes{};
    };

    class Proto
    {
    public:
        explicit Proto(std::span<const std::byte> proto_data);

        ProtoField resolve_field_by_path(std::vector<PathStep> path);
        ProtoField resolve_field_by_path(const FlatProtoPath& path);

        static constexpr int WIRE_VARINT = 0;
        static constexpr int WIRE_64BIT  = 1;
        static constexpr int WIRE_LEN    = 2;
        static constexpr int WIRE_32BIT  = 5;

        static inline uint64_t decode_varint(const std::byte* data,
                                             std::size_t size,
                                             std::size_t& pos)
        {
            uint64_t result = 0;
            int shift = 0;
            while (pos < size)
            {
                uint8_t b = static_cast<uint8_t>(data[pos++]);
                result |= static_cast<uint64_t>(b & 0x7F) << shift;
                if ((b & 0x80) == 0) return result;
                shift += 7;
                if (shift >= 64)
                    throw std::runtime_error("Varint overflow");
            }
            throw std::runtime_error("Unexpected end of buffer in varint");
        }

        template <typename Fn>
        static void for_each_field_in(std::span<const std::byte> buf, Fn&& cb)
        {
            const std::byte* data = buf.data();
            std::size_t      size = buf.size();
            std::size_t      pos  = 0;

            while (pos < size)
            {
                uint64_t tag = decode_varint(data, size, pos);
                RawWireField rwf;
                rwf.field_number = static_cast<int>(tag >> 3);
                rwf.wire_type    = static_cast<int>(tag & 0x7);

                switch (rwf.wire_type)
                {
                    case WIRE_VARINT:
                        rwf.varint_or_bits = decode_varint(data, size, pos);
                        break;

                    case WIRE_64BIT: {
                        if (pos + 8 > size)
                            throw std::runtime_error("64bit field overflows buffer");
                        uint64_t bits;
                        std::memcpy(&bits, data + pos, 8);
                        pos += 8;
                        rwf.varint_or_bits = bits;
                        break;
                    }

                    case WIRE_32BIT: {
                        if (pos + 4 > size)
                            throw std::runtime_error("32bit field overflows buffer");
                        uint32_t bits;
                        std::memcpy(&bits, data + pos, 4);
                        pos += 4;
                        rwf.varint_or_bits = bits;
                        break;
                    }

                    case WIRE_LEN: {
                        uint64_t len = decode_varint(data, size, pos);
                        if (pos + len > size)
                            throw std::runtime_error("LEN field overflows buffer");
                        rwf.len_bytes = std::span<const std::byte>(
                                data + pos, static_cast<std::size_t>(len));
                        pos += static_cast<std::size_t>(len);
                        break;
                    }

                    default:
                        throw std::runtime_error(
                                "Unknown wire type: " + std::to_string(rwf.wire_type));
                }

                cb(rwf);
            }
        }

        template <typename Fn>
        void for_each_field(Fn&& cb) const
        {
            for_each_field_in(proto_data, std::forward<Fn>(cb));
        }

    private:
        std::span<const std::byte>          proto_data;
        std::vector<std::vector<std::byte>> nested_buffers;
    };
}