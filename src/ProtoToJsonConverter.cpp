#include "../include/proto/ProtoToJsonConverter.h"
#include "../include/proto/Proto.h"

#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <vector>
#include <string>
#include <string_view>

namespace proto
{
    static int32_t zagzag32(uint32_t n) { return static_cast<int32_t>((n >> 1) ^ -(n & 1)); }
    static int64_t zagzag64(uint64_t n) { return static_cast<int64_t>((n >> 1) ^ -(n & 1)); }

    static std::string json_escape_string(std::string_view s)
    {
        std::string out;
        out.reserve(s.size() + 2);
        out.push_back('"');

        for (unsigned char c : s)
        {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b";  break;
                case '\f': out += "\\f";  break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if (c < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out += buf;
                    } else {
                        out.push_back(static_cast<char>(c));
                    }
            }
        }

        out.push_back('"');
        return out;
    }

    static std::string bytes_to_hex_literal(const std::byte* data, std::size_t size)
    {
        static const char hex[] = "0123456789abcdef";
        std::string out;
        out.reserve(size * 2 + 6);
        out.push_back('"');
        out += "\\\\x";

        for (std::size_t i = 0; i < size; ++i) {
            uint8_t b = static_cast<uint8_t>(data[i]);
            out.push_back(hex[b >> 4]);
            out.push_back(hex[b & 0x0F]);
        }

        out.push_back('"');
        return out;
    }

    template <typename T>
    static std::string emit_float(T v)
    {
        if (std::isnan(v)){
            return "\"NaN\"";
        }
        if (std::isinf(v)){
            return v < 0 ? "\"-Infinity\"" : "\"Infinity\"";
        }

        std::ostringstream os;

        os << std::setprecision(std::is_same_v<T,double> ? 17 : 9) << v;

        return os.str();
    }

    static std::string quoted_number(const std::string& num)
    {
        std::string out;

        out.reserve(num.size() + 2);
        out.push_back('"');
        out += num;
        out.push_back('"');

        return out;
    }

    static std::string emit_scalar_varint(FieldKind kind, uint64_t v)
    {
        switch (kind) {
            case FieldKind::Int32: return std::to_string(static_cast<int32_t>(v));
            case FieldKind::UInt32: return std::to_string(static_cast<uint32_t>(v));
            case FieldKind::SInt32: return std::to_string(zagzag32(static_cast<uint32_t>(v)));
            case FieldKind::Int64: return quoted_number(std::to_string(static_cast<int64_t>(v)));
            case FieldKind::UInt64: return quoted_number(std::to_string(v));
            case FieldKind::SInt64: return quoted_number(std::to_string(zagzag64(v)));
            case FieldKind::Bool: return v ? "true" : "false";
            default:
                throw std::runtime_error("emit_scalar_varint: kind mismatch (" + to_string(kind) + ")");
        }
    }

    static std::string emit_scalar_32bit(FieldKind kind, uint32_t bits)
    {
        switch (kind) {
            case FieldKind::Fixed32:{
                return std::to_string(bits);
            }
            case FieldKind::Float: {
                float f; std::memcpy(&f, &bits, 4);
                return emit_float(f);
            }
            default:{
                throw std::runtime_error("emit_scalar_32bit: kind mismatch (" + to_string(kind) + ")");
            }
        }
    }

    static std::string emit_scalar_64bit(FieldKind kind, uint64_t bits)
    {
        switch (kind) {
            case FieldKind::Fixed64: {
                return quoted_number(std::to_string(bits));
            }
            case FieldKind::Double: {
                double d; std::memcpy(&d, &bits, 8);
                return emit_float(d);
            }
            default:{
                throw std::runtime_error("emit_scalar_64bit: kind mismatch (" + to_string(kind) + ")");
            }

        }
    }

    static std::string emit_scalar_len(FieldKind kind,
                                       const std::byte* data, std::size_t size)
    {
        switch (kind) {
            case FieldKind::String:{
                return json_escape_string(std::string_view(reinterpret_cast<const char*>(data), size));
            }
            case FieldKind::Bytes:{
                return bytes_to_hex_literal(data, size);
            }
            default:{
                throw std::runtime_error("emit_scalar_len: kind mismatch (" + to_string(kind) + ")");
            }
        }
    }

    static std::vector<std::string> emit_packed_scalars(
            FieldKind kind, const std::byte* data, std::size_t size)
    {
        std::vector<std::string> out;
        std::size_t pos = 0;
        while (pos < size)
        {
            switch (kind) {
                case FieldKind::Int32:
                case FieldKind::Int64:
                case FieldKind::UInt32:
                case FieldKind::UInt64:
                case FieldKind::SInt32:
                case FieldKind::SInt64:
                case FieldKind::Bool: {
                    uint64_t v = Proto::decode_varint(data, size, pos);
                    out.push_back(emit_scalar_varint(kind, v));
                    break;
                }
                case FieldKind::Fixed32:
                case FieldKind::Float: {
                    if (pos + 4 > size){
                        throw std::runtime_error("Packed 32-bit overflows buffer");
                    }

                    uint32_t bits;
                    std::memcpy(&bits, data + pos, 4);
                    pos += 4;
                    out.push_back(emit_scalar_32bit(kind, bits));
                    break;
                }
                case FieldKind::Fixed64:
                case FieldKind::Double: {
                    if (pos + 8 > size){
                        throw std::runtime_error("Packed 64-bit overflows buffer");
                    }

                    uint64_t bits;
                    std::memcpy(&bits, data + pos, 8);
                    pos += 8;
                    out.push_back(emit_scalar_64bit(kind, bits));
                    break;
                }
                default:{
                    throw std::runtime_error("Packed encoding not valid for kind " + to_string(kind));
                }

            }
        }
        return out;
    }

    //TODO это бы куда-то перенести.
    struct FieldSlot {
        std::vector<std::string> pieces;
        bool                     seen = false;
        bool                     is_repeated = false;
        std::string_view         name;
        FieldKind                kind = FieldKind::Int32;
    };

    //TODO это бы куда-то перенести.
    struct SchemaMapAdapter {
        const ProtoSchemeMap& schema;
        const ProtoSchemeMap::MessageDef& msg;

        std::size_t schema_field_count() const { return msg.fields_in_order.size(); }

        void init_slot(std::size_t schema_idx, FieldSlot& slot) const {
            const auto& f = msg.fields_in_order[schema_idx];
            slot.name        = f.name;
            slot.kind        = f.kind;
            slot.is_repeated = f.is_repeated;
        }

        const ProtoSchemeMap::FieldDef* find_by_number(int field_number) const {
            auto it = msg.fields_by_number.find(field_number);
            return it == msg.fields_by_number.end() ? nullptr : &it->second;
        }

        int schema_index_of(const ProtoSchemeMap::FieldDef& f) const { return f.schema_index; }

        SchemaMapAdapter enter_nested(const ProtoSchemeMap::FieldDef& f) const {
            const auto* nested = schema.get_message_def(f.message_type);
            if (!nested)
                throw std::runtime_error("Unknown nested message type: " + f.message_type);
            return SchemaMapAdapter{schema, *nested};
        }
    };

    //TODO это бы куда-то перенести.
    struct SchemeFlatAdapter {
        const ProtoSchemeFlatMap& schema;
        const ProtoSchemeFlatMap::MessageRecord& msg;

        std::size_t schema_field_count() const { return msg.fields_count; }

        void init_slot(std::size_t schema_idx, FieldSlot& slot) const {
            const auto* f = schema.field_ptr(msg.fields_start + schema_idx);
            slot.name        = schema.name_of(*f);
            slot.kind        = static_cast<FieldKind>(f->kind);
            slot.is_repeated = f->is_repeated != 0;
        }

        const ProtoSchemeFlatMap::FieldRecord* find_by_number(int field_number) const {
            return schema.find_field(msg, field_number);
        }

        int schema_index_of(const ProtoSchemeFlatMap::FieldRecord& f) const {
            const auto* base = schema.field_ptr(msg.fields_start);
            return static_cast<int>(&f - base);
        }

        SchemeFlatAdapter enter_nested(const ProtoSchemeFlatMap::FieldRecord& f) const {
            if (f.nested_msg_idx < 0)
                throw std::runtime_error("enter_nested on non-message field");
            const auto& nested = schema.message_at(static_cast<std::size_t>(f.nested_msg_idx));
            return SchemeFlatAdapter{schema, nested};
        }
    };

    template <typename Adapter>
    static std::string emit_message(std::span<const std::byte> buf, const Adapter& a)
    {
        const std::size_t fcount = a.schema_field_count();
        std::vector<FieldSlot> slots(fcount);
        for (std::size_t i = 0; i < fcount; ++i)
            a.init_slot(i, slots[i]);

        Proto::for_each_field_in(buf, [&](const RawWireField& rwf)
        {
            auto def = a.find_by_number(rwf.field_number);
            if (!def) return;

            const int schema_idx = a.schema_index_of(*def);
            FieldSlot& slot = slots[schema_idx];
            slot.seen = true;
            const FieldKind kind = slot.kind;

            switch (rwf.wire_type)
            {
                case Proto::WIRE_VARINT:
                    slot.pieces.push_back(emit_scalar_varint(kind, rwf.varint_or_bits));
                    break;

                case Proto::WIRE_64BIT:
                    slot.pieces.push_back(emit_scalar_64bit(kind, rwf.varint_or_bits));
                    break;

                case Proto::WIRE_32BIT:
                    slot.pieces.push_back(emit_scalar_32bit(
                            kind, static_cast<uint32_t>(rwf.varint_or_bits)));
                    break;

                case Proto::WIRE_LEN: {
                    const std::byte* sub    = rwf.len_bytes.data();
                    const std::size_t subsz = rwf.len_bytes.size();

                    if (kind == FieldKind::Message) {
                        slot.pieces.push_back(
                                emit_message(rwf.len_bytes, a.enter_nested(*def)));
                    }
                    else if (kind == FieldKind::String || kind == FieldKind::Bytes) {
                        slot.pieces.push_back(emit_scalar_len(kind, sub, subsz));
                    }
                    else {
                        auto items = emit_packed_scalars(kind, sub, subsz);
                        for (auto& s : items) slot.pieces.push_back(std::move(s));
                    }
                    break;
                }

                default:
                    break;
            }
        });

        std::string out;
        out.push_back('{');
        bool first = true;
        for (const FieldSlot& slot : slots)
        {
            if (!slot.seen) continue;
            if (!first) out.push_back(',');
            first = false;

            out += json_escape_string(slot.name);
            out.push_back(':');

            if (slot.is_repeated) {
                out.push_back('[');
                for (std::size_t j = 0; j < slot.pieces.size(); ++j) {
                    if (j) out.push_back(',');
                    out += slot.pieces[j];
                }
                out.push_back(']');
            } else {
                if (slot.pieces.empty()) out += "null";
                else                     out += slot.pieces.back();
            }
        }
        out.push_back('}');
        return out;
    }

    std::string ProtoToJsonConverter::convert_to_json(
            std::span<const std::byte> proto_data,
            const std::string& root_message_name,
            const ProtoSchemeMap& schema)
    {
        const auto* root = schema.get_message_def(root_message_name);

        if (!root){
            throw std::runtime_error("Unknown root message: " + root_message_name);
        }

        return emit_message(proto_data, SchemaMapAdapter{schema, *root});
    }

    std::string ProtoToJsonConverter::convert_to_json(
            std::span<const std::byte> proto_data,
            const ProtoSchemeFlatMap& schema)
    {
        const auto& root = schema.message_at(schema.root_message_idx());
        return emit_message(proto_data, SchemeFlatAdapter{schema, root});
    }
}