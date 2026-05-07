#include "../include/proto/Proto.h"
#include "proto/Proto.h"
#include <stdexcept>
#include <cstring>
#include <cstddef>
#include <cstdint>

namespace proto
{
    static constexpr int WIRE_VARINT   = 0;
    static constexpr int WIRE_64BIT    = 1;
    static constexpr int WIRE_LEN      = 2;
    static constexpr int WIRE_32BIT    = 5;

    static uint64_t decode_varint(const std::byte* data, size_t size, size_t& pos)
    {
        uint64_t result = 0;
        int shift = 0;
        while (pos < size)
        {
            uint8_t b = static_cast<uint8_t>(data[pos++]);
            result |= static_cast<uint64_t>(b & 0x7F) << shift;
            if ((b & 0x80) == 0) return result;
            shift += 7;
            if (shift >= 64) throw std::runtime_error("Varint overflow");
        }
        throw std::runtime_error("Unexpected end of buffer in varint");
    }

    static int32_t zagzag32(uint32_t n) { return static_cast<int32_t>((n >> 1) ^ -(n & 1)); }
    static int64_t zagzag64(uint64_t n) { return static_cast<int64_t>((n >> 1) ^ -(n & 1)); }

    Proto::Proto(std::span<const std::byte> data)
            : proto_data(data) {}

    struct RawField {
        int wire_type = -1;
        uint64_t varint_val = 0;
        std::vector<std::byte> len_val;
        bool found = false;
    };

    static RawField find_field(const std::byte* data, size_t size, int target_field_number)
    {
        size_t pos = 0;
        while (pos < size)
        {
            uint64_t tag    = decode_varint(data, size, pos);
            int field_num   = static_cast<int>(tag >> 3);
            int wire_type   = static_cast<int>(tag & 0x7);

            if (field_num == target_field_number)
            {
                RawField rf;
                rf.wire_type = wire_type;
                rf.found = true;

                if (wire_type == WIRE_VARINT)
                {
                    rf.varint_val = decode_varint(data, size, pos);
                }
                else if (wire_type == WIRE_LEN)
                {
                    uint64_t len = decode_varint(data, size, pos);
                    if (pos + len > size) throw std::runtime_error("LEN field overflows buffer");
                    rf.len_val.assign(
                            data + pos,
                            data + pos + len
                    );
                    pos += len;
                }
                else if (wire_type == WIRE_64BIT)
                {
                    if (pos + 8 > size) throw std::runtime_error("64bit field overflows buffer");
                    std::memcpy(&rf.varint_val, data + pos, 8);
                    pos += 8;
                }
                else if (wire_type == WIRE_32BIT)
                {
                    uint32_t v = 0;
                    if (pos + 4 > size) throw std::runtime_error("32bit field overflows buffer");
                    std::memcpy(&v, data + pos, 4);
                    rf.varint_val = v;
                    pos += 4;
                }
                return rf;
            }
            else
            {
                if (wire_type == WIRE_VARINT)
                    decode_varint(data, size, pos);
                else if (wire_type == WIRE_LEN)
                {
                    uint64_t len = decode_varint(data, size, pos);
                    pos += len;
                }
                else if (wire_type == WIRE_64BIT) pos += 8;
                else if (wire_type == WIRE_32BIT) pos += 4;
                else throw std::runtime_error("Unknown wire type: " + std::to_string(wire_type));
            }
        }
        return {};
    }

    ProtoField Proto::resolve_field_by_path(std::vector<PathStep> path)
    {
        if (path.empty())
            throw std::runtime_error("Empty path");

        const std::byte* cur_data = proto_data.data();
        size_t           cur_size = proto_data.size();

        // Идём по всем шагам кроме последнего — они должны быть Message
        for (size_t i = 0; i + 1 < path.size(); ++i)
        {
            const PathStep& step = path[i];
            if (step.kind != FieldKind::Message)
                throw std::runtime_error("Intermediate path step '" + step.field_name + "' is not a Message");

            RawField rf = find_field(cur_data, cur_size, step.field_number);
            if (!rf.found)
                throw std::runtime_error("Field '" + step.field_name + "' not found in buffer");
            if (rf.wire_type != WIRE_LEN)
                throw std::runtime_error("Expected LEN wire type for message field '" + step.field_name + "'");

            // Переходим внутрь вложенного сообщения
            // Храним промежуточные буферы чтобы не висели в стеке
            nested_buffers.push_back(std::move(rf.len_val));
            cur_data = nested_buffers.back().data();
            cur_size = nested_buffers.back().size();
        }

        // Последний шаг — целевое поле
        const PathStep& last = path.back();
        RawField rf = find_field(cur_data, cur_size, last.field_number);
        if (!rf.found)
            throw std::runtime_error("Field '" + last.field_name + "' not found in buffer");

        ProtoField result;
        result.field_name   = last.field_name;
        result.field_number = last.field_number;
        result.kind         = last.kind;

        switch (last.kind)
        {
            case FieldKind::Int32:
                result.value = static_cast<int32_t>(rf.varint_val); break;
            case FieldKind::Int64:
                result.value = static_cast<int64_t>(rf.varint_val); break;
            case FieldKind::UInt32:
                result.value = static_cast<uint32_t>(rf.varint_val); break;
            case FieldKind::UInt64:
                result.value = rf.varint_val; break;
            case FieldKind::SInt32:
                result.value = zagzag32(static_cast<uint32_t>(rf.varint_val)); break;
            case FieldKind::SInt64:
                result.value = zagzag64(rf.varint_val); break;
            case FieldKind::Bool:
                result.value = static_cast<bool>(rf.varint_val); break;
            case FieldKind::Float: {
                float f; std::memcpy(&f, &rf.varint_val, 4);
                result.value = f; break;
            }
            case FieldKind::Double: {
                double d; std::memcpy(&d, &rf.varint_val, 8);
                result.value = d; break;
            }
            case FieldKind::Fixed32:
                result.value = static_cast<uint32_t>(rf.varint_val); break;
            case FieldKind::Fixed64:
                result.value = rf.varint_val; break;
            case FieldKind::String: {
                std::string s(reinterpret_cast<const char*>(rf.len_val.data()), rf.len_val.size());
                result.value = std::move(s); break;
            }
            case FieldKind::Bytes:
            case FieldKind::Message:
                result.value = std::move(rf.len_val); break;
        }

        nested_buffers.clear();
        return result;
    }

} // namespace proto