#include "../include/proto/Proto.h"
#include "proto/Proto.h"
#include <stdexcept>
#include <cstring>
#include <cstddef>
#include <cstdint>

namespace proto
{

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

    static RawField find_field(
            const std::byte* data,
            size_t size,
            int target_field_number)
    {
        size_t pos = 0;
        while (pos < size)
        {
            uint64_t tag = Proto::decode_varint(data, size, pos);
            int field_num = static_cast<int>(tag >> 3);
            int wire_type = static_cast<int>(tag & 0x7);

            if (field_num == target_field_number)
            {
                RawField rf;
                rf.wire_type = wire_type;
                rf.found = true;

                if (wire_type == Proto::WIRE_VARINT)
                {
                    rf.varint_val = Proto::decode_varint(data, size, pos);
                }
                else if (wire_type == Proto::WIRE_LEN)
                {
                    uint64_t len = Proto::decode_varint(data, size, pos);

                    if (pos + len > size){
                        throw std::runtime_error("LEN field overflows buffer");
                    }

                    rf.len_val.assign(
                            data + pos,
                            data + pos + len
                    );
                    pos += len;
                }
                else if (wire_type == Proto::WIRE_64BIT)
                {
                    if (pos + 8 > size){
                        throw std::runtime_error("64bit field overflows buffer");
                    }

                    std::memcpy(&rf.varint_val, data + pos, 8);
                    pos += 8;
                }
                else if (wire_type == Proto::WIRE_32BIT)
                {
                    uint32_t v = 0;

                    if (pos + 4 > size){
                        throw std::runtime_error("32bit field overflows buffer");
                    }

                    std::memcpy(&v, data + pos, 4);
                    rf.varint_val = v;
                    pos += 4;
                }
                return rf;
            }
            else
            {
                if (wire_type == Proto::WIRE_VARINT){
                    Proto::decode_varint(data, size, pos);
                }

                else if (wire_type == Proto::WIRE_LEN)
                {
                    uint64_t len = Proto::decode_varint(data, size, pos);
                    pos += len;
                }
                else if (wire_type == Proto::WIRE_64BIT){
                    pos += 8;
                }
                else if (wire_type == Proto::WIRE_32BIT){
                    pos += 4;
                }
                else{
                    throw std::runtime_error("Unknown wire type: " + std::to_string(wire_type));
                }
            }
        }
        return {};
    }

    ProtoField Proto::resolve_field_by_path(std::vector<PathStep> path)
    {
        if (path.empty()){
            throw std::runtime_error("Empty path");
        }

        const std::byte* cur_data = proto_data.data();
        size_t cur_size = proto_data.size();

        for (size_t i = 0; i + 1 < path.size(); ++i)
        {
            const PathStep& step = path[i];
            if (step.kind != FieldKind::Message){
                throw std::runtime_error("Intermediate path step '" + step.field_name + "' is not a Message");
            }

            RawField rf = find_field(cur_data, cur_size, step.field_number);

            if (!rf.found){
                throw std::runtime_error("Field '" + step.field_name + "' not found in buffer");
            }

            if (rf.wire_type != Proto::WIRE_LEN){
                throw std::runtime_error("Expected LEN wire type for message field '" + step.field_name + "'");
            }

            nested_buffers.push_back(std::move(rf.len_val));
            cur_data = nested_buffers.back().data();
            cur_size = nested_buffers.back().size();
        }

        const PathStep& last = path.back();
        RawField rf = find_field(cur_data, cur_size, last.field_number);

        if (!rf.found){
            throw std::runtime_error("Field '" + last.field_name + "' not found in buffer");
        }

        ProtoField result;
        result.field_name = last.field_name;
        result.field_number = last.field_number;
        result.kind = last.kind;

        switch (last.kind)
        {
            case FieldKind::Int32:{
                result.value = static_cast<int32_t>(rf.varint_val);
                break;
            }
            case FieldKind::Int64:{
                result.value = static_cast<int64_t>(rf.varint_val);
                break;
            }
            case FieldKind::UInt32:{
                result.value = static_cast<uint32_t>(rf.varint_val);
                break;
            }
            case FieldKind::UInt64:{
                result.value = rf.varint_val;
                break;
            }
            case FieldKind::SInt32:{
                result.value = zagzag32(static_cast<uint32_t>(rf.varint_val));
                break;
            }
            case FieldKind::SInt64:{
                result.value = zagzag64(rf.varint_val);
                break;
            }
            case FieldKind::Bool:{
                result.value = static_cast<bool>(rf.varint_val);
                break;
            }
            case FieldKind::Float: {
                float f;

                std::memcpy(&f, &rf.varint_val, 4);
                result.value = f;
                break;
            }
            case FieldKind::Double: {
                double d;

                std::memcpy(&d, &rf.varint_val, 8);
                result.value = d;
                break;
            }
            case FieldKind::Fixed32: {
                result.value = static_cast<uint32_t>(rf.varint_val);
                break;
            }
            case FieldKind::Fixed64:{
                result.value = rf.varint_val;
                break;
            }
            case FieldKind::String: {
                std::string s(reinterpret_cast<const char*>(rf.len_val.data()), rf.len_val.size());

                result.value = std::move(s);
                break;
            }
            case FieldKind::Bytes: //тут проваливаемся ниже в блок message
            case FieldKind::Message:{
                result.value = std::move(rf.len_val); break;
            }
        }

        nested_buffers.clear();
        return result;
    }

    ProtoField Proto::resolve_field_by_path(const FlatProtoPath& path)
    {
        if (path.empty())
            throw std::runtime_error("Empty path");

        const std::byte* cur_data = proto_data.data();
        size_t cur_size = proto_data.size();

        const std::size_t n = path.size();
        for (std::size_t i = 0; i + 1 < n; ++i)
        {
            const FlatPathStep step = path[i];
            if (step.kind != FieldKind::Message){
                throw std::runtime_error(
                        "Intermediate path step '" + std::string(path.name_of(i)) + "' is not a Message");
            }

            RawField rf = find_field(cur_data, cur_size, step.field_number);
            if (!rf.found){
                throw std::runtime_error("Field '" + std::string(path.name_of(i)) + "' not found in buffer");
            }

            if (rf.wire_type != Proto::WIRE_LEN){
                throw std::runtime_error(
                        "Expected LEN wire type for message field '" +
                        std::string(path.name_of(i)) + "'");
            }

            nested_buffers.push_back(std::move(rf.len_val));
            cur_data = nested_buffers.back().data();
            cur_size = nested_buffers.back().size();
        }

        const FlatPathStep last = path[n - 1];
        RawField rf = find_field(cur_data, cur_size, last.field_number);
        if (!rf.found){
            throw std::runtime_error(
                    "Field '" + std::string(path.name_of(n - 1)) + "' not found in buffer");
        }

        ProtoField result;
        result.field_number = last.field_number;
        result.kind = last.kind;

        switch (last.kind)
        {
            case FieldKind::Int32: {
                result.value = static_cast<int32_t>(rf.varint_val);
                break;
            }
            case FieldKind::Int64: {
                result.value = static_cast<int64_t>(rf.varint_val);
                break;
            }
            case FieldKind::UInt32: {
                result.value = static_cast<uint32_t>(rf.varint_val);
                break;
            }
            case FieldKind::UInt64: {
                result.value = rf.varint_val;
                break;
            }
            case FieldKind::SInt32: {
                result.value = zagzag32(static_cast<uint32_t>(rf.varint_val));
                break;
            }
            case FieldKind::SInt64: {
                result.value = zagzag64(rf.varint_val);
                break;
            }
            case FieldKind::Bool: {
                result.value = static_cast<bool>(rf.varint_val);
                break;
            }
            case FieldKind::Float: {
                float f; std::memcpy(&f, &rf.varint_val, 4);
                result.value = f;
                break;
            }
            case FieldKind::Double: {
                double d; std::memcpy(&d, &rf.varint_val, 8);
                result.value = d;
                break;
            }
            case FieldKind::Fixed32: {
                result.value = static_cast<uint32_t>(rf.varint_val);
                break;
            }
            case FieldKind::Fixed64: {
                result.value = rf.varint_val;
                break;
            }
            case FieldKind::String: {
                std::string s(reinterpret_cast<const char*>(rf.len_val.data()),rf.len_val.size());
                result.value = std::move(s);
                break;
            }
            case FieldKind::Bytes: //тут провалится должны в мессадж блок
            case FieldKind::Message:
                result.value = std::move(rf.len_val);
                break;
        }

        nested_buffers.clear();
        return result;
    }
}