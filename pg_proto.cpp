extern "C" {
#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include <varatt.h>

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(pg_proto_version);

PG_FUNCTION_INFO_V1(get_text_by_scheme);
PG_FUNCTION_INFO_V1(get_int32_by_scheme);
PG_FUNCTION_INFO_V1(get_int64_by_scheme);
PG_FUNCTION_INFO_V1(get_double_by_scheme);
PG_FUNCTION_INFO_V1(get_float_by_scheme);
}

#include "pg_proto.h"
#include "proto/Proto.h"
#include "proto/ProtoSchemaMap.h"

using namespace proto;

std::span<const std::byte> convertBytesSpanFromDto(const bytea* bytesDto){
    const auto* raw_ptr =
            reinterpret_cast<const std::byte*>(VARDATA_ANY(bytesDto));
    const auto raw_size =
            static_cast<std::size_t>(VARSIZE_ANY_EXHDR(bytesDto));

    std::span<const std::byte> result(raw_ptr, raw_size);

    return result;
}

std::string convertStringFromDto(const text* textDto){
    char* cstr = text_to_cstring(textDto);
    std::string result(cstr);
    pfree(cstr);

    return result;
}

Datum pg_proto_version(PG_FUNCTION_ARGS)
{
    std::string str = "pg_proto 0.1";

    PG_RETURN_TEXT_P(cstring_to_text_with_len(str.c_str(), str.size()));
}

Datum get_text_by_scheme(PG_FUNCTION_ARGS)
{
    text* proto_schema_dto = PG_GETARG_TEXT_PP(0);
    text* root_message_dto = PG_GETARG_TEXT_PP(1);
    text* proto_command_dto = PG_GETARG_TEXT_PP(2);
    bytea* proto_data_dto = PG_GETARG_BYTEA_PP(3);

    std::span<const std::byte> proto_data_raw = convertBytesSpanFromDto(proto_data_dto);
    std::string proto_schema_raw = convertStringFromDto(proto_schema_dto);
    std::string proto_command = convertStringFromDto(proto_command_dto);
    std::string root_message = convertStringFromDto(root_message_dto);

    Proto proto_data(proto_data_raw);
    ProtoSchemaMap proto_schema_map(proto_schema_raw);

    std::vector<PathStep> path = proto_schema_map.resolve_path(root_message, proto_command);

    ProtoField field = proto_data.resolve_field_by_path(path);

    if(field.kind != FieldKind::String){
        std::string message = "Invalid Field type: " + to_string(field.kind);

        ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("%s",message.c_str())));
    }

    std::string result = std::get<std::string>(field.value);

    PG_RETURN_TEXT_P(cstring_to_text_with_len(result.c_str(), result.size()));
}

Datum get_int32_by_scheme(PG_FUNCTION_ARGS)
{
    text* proto_schema_dto = PG_GETARG_TEXT_PP(0);
    text* root_message_dto = PG_GETARG_TEXT_PP(1);
    text* proto_command_dto = PG_GETARG_TEXT_PP(2);
    bytea* proto_data_dto = PG_GETARG_BYTEA_PP(3);

    std::span<const std::byte> proto_data_raw = convertBytesSpanFromDto(proto_data_dto);
    std::string proto_schema_raw = convertStringFromDto(proto_schema_dto);
    std::string proto_command = convertStringFromDto(proto_command_dto);
    std::string root_message = convertStringFromDto(root_message_dto);

    Proto proto_data(proto_data_raw);
    ProtoSchemaMap proto_schema_map(proto_schema_raw);

    std::vector<PathStep> path = proto_schema_map.resolve_path(root_message, proto_command);

    ProtoField field = proto_data.resolve_field_by_path(path);

    if(field.kind != FieldKind::Int32 & field.kind != FieldKind::SInt32 & field.kind != FieldKind::UInt32){
        std::string message = "Invalid Field type: " + to_string(field.kind);

        ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("%s",message.c_str())));
    }

    if (field.kind == FieldKind::UInt32){
        uint32_t result_u = std::get<uint32_t>(field.value);

        PG_RETURN_INT32(result_u);
    }

    int32_t result = std::get<int32_t>(field.value);

    PG_RETURN_INT32(result);
}

Datum get_int64_by_scheme(PG_FUNCTION_ARGS)
{
    text* proto_schema_dto = PG_GETARG_TEXT_PP(0);
    text* root_message_dto = PG_GETARG_TEXT_PP(1);
    text* proto_command_dto = PG_GETARG_TEXT_PP(2);
    bytea* proto_data_dto = PG_GETARG_BYTEA_PP(3);

    std::span<const std::byte> proto_data_raw = convertBytesSpanFromDto(proto_data_dto);
    std::string proto_schema_raw = convertStringFromDto(proto_schema_dto);
    std::string proto_command = convertStringFromDto(proto_command_dto);
    std::string root_message = convertStringFromDto(root_message_dto);

    Proto proto_data(proto_data_raw);
    ProtoSchemaMap proto_schema_map(proto_schema_raw);

    std::vector<PathStep> path = proto_schema_map.resolve_path(root_message, proto_command);

    ProtoField field = proto_data.resolve_field_by_path(path);

    if(field.kind != FieldKind::Int64 & field.kind != FieldKind::SInt64 & field.kind != FieldKind::UInt64){
        std::string message = "Invalid Field type: " + to_string(field.kind);

        ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("%s",message.c_str())));
    }

    if (field.kind == FieldKind::UInt64){
        uint64_t result_u = std::get<uint64_t>(field.value);

        PG_RETURN_INT64(result_u);
    }

    int64_t result = std::get<int64_t>(field.value);

    PG_RETURN_INT64(result);
}

Datum get_double_by_scheme(PG_FUNCTION_ARGS)
{
    text* proto_schema_dto = PG_GETARG_TEXT_PP(0);
    text* root_message_dto = PG_GETARG_TEXT_PP(1);
    text* proto_command_dto = PG_GETARG_TEXT_PP(2);
    bytea* proto_data_dto = PG_GETARG_BYTEA_PP(3);

    std::span<const std::byte> proto_data_raw = convertBytesSpanFromDto(proto_data_dto);
    std::string proto_schema_raw = convertStringFromDto(proto_schema_dto);
    std::string proto_command = convertStringFromDto(proto_command_dto);
    std::string root_message = convertStringFromDto(root_message_dto);

    Proto proto_data(proto_data_raw);
    ProtoSchemaMap proto_schema_map(proto_schema_raw);

    std::vector<PathStep> path = proto_schema_map.resolve_path(root_message, proto_command);

    ProtoField field = proto_data.resolve_field_by_path(path);

    if(field.kind != FieldKind::Double){
        std::string message = "Invalid Field type: " + to_string(field.kind);

        ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("%s",message.c_str())));
    }

    double result = std::get<double>(field.value);

    PG_RETURN_FLOAT8(result);
}

Datum get_float_by_scheme(PG_FUNCTION_ARGS)
{
    text* proto_schema_dto = PG_GETARG_TEXT_PP(0);
    text* root_message_dto = PG_GETARG_TEXT_PP(1);
    text* proto_command_dto = PG_GETARG_TEXT_PP(2);
    bytea* proto_data_dto = PG_GETARG_BYTEA_PP(3);

    std::span<const std::byte> proto_data_raw = convertBytesSpanFromDto(proto_data_dto);
    std::string proto_schema_raw = convertStringFromDto(proto_schema_dto);
    std::string proto_command = convertStringFromDto(proto_command_dto);
    std::string root_message = convertStringFromDto(root_message_dto);

    Proto proto_data(proto_data_raw);
    ProtoSchemaMap proto_schema_map(proto_schema_raw);

    std::vector<PathStep> path = proto_schema_map.resolve_path(root_message, proto_command);

    ProtoField field = proto_data.resolve_field_by_path(path);

    if(field.kind != FieldKind::Float){
        std::string message = "Invalid Field type: " + to_string(field.kind);

        ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("%s",message.c_str())));
    }

    float result = std::get<float>(field.value);

    PG_RETURN_FLOAT4(result);
}
