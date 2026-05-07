#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "ProtoUtils.h"

namespace proto
{
    class ProtoSchemaMap
    {
    public:
        explicit ProtoSchemaMap(std::string proto_scheme_text);

        std::vector<PathStep> resolve_path(
                const std::string& root_message,
                const std::string& command
        );

    private:
        struct FieldDef {
            std::string name;
            int number = 0;
            FieldKind kind;
            std::string message_type;
        };

        struct MessageDef {
            std::string name;
            std::unordered_map<std::string, FieldDef> fields_by_name;
        };

        std::unordered_map<std::string, MessageDef> messages;
    };
}