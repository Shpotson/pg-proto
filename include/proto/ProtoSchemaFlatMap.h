#pragma once

#include <string>
#include <vector>
#include <span>

namespace proto
{
    class ProtoSchema
    {
        public:
        explicit ProtoSchema(std::span<const std::byte> schema_data);

        private:
        std::span<const std::byte> proto_data schema_data;
    };
}