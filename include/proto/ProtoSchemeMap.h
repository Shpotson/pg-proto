#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "ProtoUtils.h"

namespace proto
{
    class ProtoSchemeMap
    {
    public:
        explicit ProtoSchemeMap(std::string proto_scheme_text);

        std::vector<PathStep> resolve_path(
                const std::string& root_message,
                const std::string& command
        );

        struct FieldDef {
            std::string name;
            int         number = 0;
            FieldKind   kind;
            std::string message_type;
            bool        is_repeated = false;
            int         schema_index = 0;
        };

        struct MessageDef {
            std::string name;
            std::unordered_map<std::string, FieldDef> fields_by_name;
            std::unordered_map<int, FieldDef>         fields_by_number;
            std::vector<FieldDef>                     fields_in_order;
        };

        const MessageDef* get_message_def(const std::string& name) const;

    private:
        std::unordered_map<std::string, MessageDef> messages;
    };
}