#include "../include/proto/ProtoSchemeMap.h"

#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace proto
{

    static std::string trim(const std::string& s)
    {
        size_t b = s.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) return {};
        size_t e = s.find_last_not_of(" \t\r\n");
        return s.substr(b, e - b + 1);
    }

    static std::string strip_comment(const std::string& s)
    {
        auto pos = s.find("//");
        if (pos == std::string::npos) return s;
        return s.substr(0, pos);
    }

    static FieldKind kind_from_string(const std::string& t)
    {
        if (t == "int32") return FieldKind::Int32;
        if (t == "int64") return FieldKind::Int64;
        if (t == "uint32") return FieldKind::UInt32;
        if (t == "uint64") return FieldKind::UInt64;
        if (t == "sint32") return FieldKind::SInt32;
        if (t == "sint64") return FieldKind::SInt64;
        if (t == "bool") return FieldKind::Bool;
        if (t == "string") return FieldKind::String;
        if (t == "bytes") return FieldKind::Bytes;
        if (t == "fixed32") return FieldKind::Fixed32;
        if (t == "fixed64") return FieldKind::Fixed64;
        if (t == "float") return FieldKind::Float;
        if (t == "double") return FieldKind::Double;

        return FieldKind::Message;
    }

    static std::vector<std::string> split_tokens(const std::string& s)
    {
        std::vector<std::string> tokens;
        std::istringstream ss(s);
        std::string tok;

        while (ss >> tok){
            tokens.push_back(tok);
        }
        return tokens;
    }


    ProtoSchemeMap::ProtoSchemeMap(std::string proto_scheme_text)
    {
        std::istringstream stream(proto_scheme_text);
        std::string line;

        MessageDef* current_message = nullptr;

        while (std::getline(stream, line))
        {
            line = trim(strip_comment(line));
            if (line.empty()){
                continue;
            }

            if (line.rfind("syntax", 0) == 0 ||
                line.rfind("package", 0) == 0 ||
                line.rfind("option", 0) == 0 ||
                line.rfind("import", 0) == 0){
                continue;
            }


            if (line.rfind("message ", 0) == 0)
            {
                size_t brace = line.find('{');

                std::string msg_name = trim(line.substr(
                        8,
                        brace == std::string::npos ? std::string::npos : brace - 8));

                MessageDef def;
                def.name = msg_name;
                messages[msg_name] = std::move(def);
                current_message = &messages[msg_name];

                continue;
            }

            if (line == "}")
            {
                current_message = nullptr;
                continue;
            }

            if (current_message == nullptr){
                continue;
            }

            if (!line.empty() && line.back() == ';'){
                line.pop_back();
            }

            bool was_repeated = false;

            for (auto prefix : {"repeated ", "optional ", "required "})
            {
                if (line.rfind(prefix, 0) == 0)
                {
                    if (std::string(prefix) == "repeated ")
                        was_repeated = true;
                    line = line.substr(std::string(prefix).size());

                    break;
                }
            }

            auto tokens = split_tokens(line);
            if (tokens.size() < 4){
                continue;
            }
            if (tokens[2] != "="){
                continue;
            }

            FieldDef field;
            field.name = tokens[1];
            field.number = std::stoi(tokens[3]);
            field.kind = kind_from_string(tokens[0]);
            field.is_repeated = was_repeated;
            field.schema_index = static_cast<int>(current_message->fields_in_order.size());

            if (field.kind == FieldKind::Message){
                field.message_type = tokens[0];
            }

            current_message->fields_by_name[field.name] = field;
            current_message->fields_by_number[field.number] = field;
            current_message->fields_in_order.push_back(std::move(field));
        }
    }

    std::vector<PathStep> ProtoSchemeMap::resolve_path(
            const std::string& root_message,
            const std::string& command)
    {
        std::string path = command;
        auto prefix_dot = path.find('.');

        if (prefix_dot != std::string::npos && path[0] == '%'){
            path = path.substr(prefix_dot + 1);
        }

        std::vector<std::string> parts;
        std::istringstream ps(path);
        std::string part;

        while (std::getline(ps, part, '.')){
            if (!part.empty()) parts.push_back(part);
        }

        std::vector<PathStep> result;
        std::string current_msg = root_message;

        for (const auto& field_name : parts)
        {
            auto msg_it = messages.find(current_msg);

            if (msg_it == messages.end()){
                throw std::runtime_error("Unknown message type: " + current_msg);
            }

            const MessageDef& msg_def = msg_it->second;
            auto field_it = msg_def.fields_by_name.find(field_name);

            if (field_it == msg_def.fields_by_name.end()){
                throw std::runtime_error("Field '" + field_name + "' not found in message '" + current_msg + "'");
            }

            const FieldDef& f = field_it->second;

            PathStep step;
            step.field_name = f.name;
            step.field_number = f.number;
            step.kind = f.kind;
            step.nested_message_type = f.message_type;

            result.push_back(step);

            if (f.kind == FieldKind::Message){
                current_msg = f.message_type;
            }
        }

        return result;
    }

    const ProtoSchemeMap::MessageDef*
    ProtoSchemeMap::get_message_def(const std::string& name) const
    {
        auto it = messages.find(name);

        if (it == messages.end()) {
            return nullptr;
        }

        return &it->second;
    }
}
