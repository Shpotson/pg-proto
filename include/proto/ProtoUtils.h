//
// Created by valer on 02.05.2026.
//

#include <string>
#include <variant>
#include <vector>
#include <cstdint>
#include <cstddef>

#ifndef PG_PROTO_PROTOUTILS_H
#define PG_PROTO_PROTOUTILS_H

enum class FieldKind {
    Int32,
    Int64,
    UInt32,
    UInt64,
    SInt32,
    SInt64,
    Bool,
    String,
    Bytes,
    Fixed32,
    Fixed64,
    Float,
    Double,
    Message
};

struct PathStep {
    std::string field_name;
    int field_number = 0;
    FieldKind kind;
    std::string nested_message_type;
};

using ProtoValue = std::variant<
        int32_t,          // Int32, SInt32
        int64_t,          // Int64, SInt64
        uint32_t,         // UInt32, Fixed32
        uint64_t,         // UInt64, Fixed64
        float,            // Float
        double,           // Double
        bool,             // Bool
        std::string,      // String
        std::vector<std::byte>  // Bytes или вложенный Message (сырые байты)
>;

struct ProtoField {
    std::string field_name;
    int         field_number = 0;
    FieldKind   kind;
    ProtoValue  value;
};

inline std::string to_string(FieldKind kind) {
    switch (kind) {
        case FieldKind::Int32:   return "Int32";
        case FieldKind::Int64:   return "Int64";
        case FieldKind::UInt32:  return "UInt32";
        case FieldKind::UInt64:  return "UInt64";
        case FieldKind::SInt32:  return "SInt32";
        case FieldKind::SInt64:  return "SInt64";
        case FieldKind::Bool:    return "Bool";
        case FieldKind::String:  return "String";
        case FieldKind::Bytes:   return "Bytes";
        case FieldKind::Fixed32: return "Fixed32";
        case FieldKind::Fixed64: return "Fixed64";
        case FieldKind::Float:   return "Float";
        case FieldKind::Double:  return "Double";
        case FieldKind::Message: return "Message";
    }
    return "Unknown";
}

#endif //PG_PROTO_PROTOUTILS_H




