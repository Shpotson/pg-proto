#pragma once

#include <string>
#include <span>
#include <cstddef>

#include "ProtoSchemeMap.h"
#include "ProtoSchemeFlatMap.h"

namespace proto
{
    class ProtoToJsonConverter
    {
    public:
        static std::string convert_to_json(
                std::span<const std::byte> proto_data,
                const std::string& root_message_name,
                const ProtoSchemeMap& schema);

        static std::string convert_to_json(
                std::span<const std::byte> proto_data,
                const ProtoSchemeFlatMap& schema);
    };
}