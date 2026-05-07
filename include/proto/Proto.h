#pragma once

#include <string>
#include <vector>
#include <span>
#include "ProtoUtils.h"

namespace proto
{
    class Proto
    {
    public:
        explicit Proto(std::span<const std::byte> proto_data);

        ProtoField resolve_field_by_path(std::vector<PathStep> path);

    private:
        std::span<const std::byte>         proto_data;
        std::vector<std::vector<std::byte>> nested_buffers;
    };
}