#include "proto/ProtoSchemeMap.h"
#include "proto/ProtoSchemeFlatMap.h"
#include <cstring>
#include <stdexcept>
#include <unordered_map>
#include <queue>
#include <cstdint>

namespace proto
{
    using proto_scheme_flat_map_detail::Header;
    using proto_scheme_flat_map_detail::MAGIC;
    using proto_scheme_flat_map_detail::VERSION;

    static inline void write_bytes(std::byte* dst, const void* src, std::size_t n)
    {
        std::memcpy(dst, src, n);
    }

    struct PlanMsg {
        const ProtoSchemeMap::MessageDef* def;
        uint32_t flat_index;
    };

    struct Plan {
        std::vector<PlanMsg> ordered_messages;
        std::unordered_map<std::string, uint32_t> name_to_index;
        std::size_t names_total = 0;
        std::size_t fields_total = 0;
    };

    static Plan build_plan(
            const ProtoSchemeMap& schema,
            const std::string& root_message_name)
    {
        const auto* root = schema.get_message_def(root_message_name);

        if (!root){
            throw std::runtime_error("ProtoSchemeFlatMap: unknown root message '" + root_message_name + "'");
        }

        Plan plan;
        std::queue<const ProtoSchemeMap::MessageDef*> q;

        auto enqueue = [&](const ProtoSchemeMap::MessageDef* m) {
            if (plan.name_to_index.count(m->name)){
                return;
            }
            uint32_t idx = static_cast<uint32_t>(plan.ordered_messages.size());

            plan.name_to_index.emplace(m->name, idx);
            plan.ordered_messages.push_back({m, idx});
            plan.names_total += m->name.size();
            plan.fields_total += m->fields_in_order.size();

            for (const auto& f : m->fields_in_order){
                plan.names_total += f.name.size();
            }

            q.push(m);
        };

        enqueue(root);

        while (!q.empty())
        {
            const ProtoSchemeMap::MessageDef* m = q.front();
            q.pop();

            for (const auto& f : m->fields_in_order)
            {
                if (f.kind != FieldKind::Message){
                    continue;
                }

                const auto* nested = schema.get_message_def(f.message_type);

                if (!nested){
                    throw std::runtime_error(
                            "ProtoSchemeFlatMap: unknown nested message type '" +
                            f.message_type + "' referenced from '" + m->name + "'");
                }

                enqueue(nested);
            }
        }

        return plan;
    }

    std::size_t ProtoSchemeFlatMap::compute_size(
            const ProtoSchemeMap& built_map,
            const std::string& root_message_name)
    {
        Plan plan = build_plan(built_map, root_message_name);
        std::size_t size = sizeof(Header);

        size += plan.ordered_messages.size() * sizeof(MessageRecord);
        size += plan.fields_total * sizeof(FieldRecord);
        size += plan.names_total;
        return size;
    }

    void ProtoSchemeFlatMap::encode_into(
            const ProtoSchemeMap& built_map,
            const std::string& root_message_name,
            std::byte* dst,
            std::size_t dst_size)
    {
        Plan plan = build_plan(built_map, root_message_name);

        const uint32_t message_count = static_cast<uint32_t>(plan.ordered_messages.size());
        const uint32_t field_count = static_cast<uint32_t>(plan.fields_total);

        const uint32_t messages_offset = sizeof(Header);
        const uint32_t fields_offset = messages_offset + message_count * sizeof(MessageRecord);
        const uint32_t names_offset = fields_offset + field_count * sizeof(FieldRecord);
        const std::size_t need = static_cast<std::size_t>(names_offset) + plan.names_total;

        if (dst_size < need)
            throw std::runtime_error("ProtoSchemeFlatMap::encode_into: dst buffer too small");

        Header h{};
        h.magic = MAGIC;
        h.version = VERSION;
        h.message_count = message_count;
        h.messages_offset = messages_offset;
        h.field_count = field_count;
        h.fields_offset = fields_offset;
        h.names_offset = names_offset;
        h.root_message_idx = 0;
        write_bytes(dst, &h, sizeof(h));

        uint32_t cur_name_off = names_offset;
        uint32_t cur_field_idx = 0;

        for (uint32_t mi = 0; mi < message_count; ++mi)
        {
            const ProtoSchemeMap::MessageDef* m = plan.ordered_messages[mi].def;

            MessageRecord mrec{};
            mrec.fields_start = cur_field_idx;
            mrec.fields_count = static_cast<uint32_t>(m->fields_in_order.size());
            mrec.name_offset = cur_name_off;
            mrec.name_len = static_cast<uint16_t>(m->name.size());
            mrec.reserved = 0;
            write_bytes(
                    dst + messages_offset + mi * sizeof(MessageRecord),
                    &mrec,
                    sizeof(mrec));

            if (mrec.name_len > 0){
                write_bytes(dst + cur_name_off, m->name.data(), mrec.name_len);
            }

            cur_name_off += mrec.name_len;

            for (uint32_t fi = 0; fi < mrec.fields_count; ++fi)
            {
                const auto& src = m->fields_in_order[fi];

                FieldRecord frec{};
                frec.field_number = src.number;
                frec.kind = static_cast<uint8_t>(src.kind);
                frec.is_repeated = src.is_repeated ? 1u : 0u;
                frec.nested_msg_idx = -1;

                if (src.kind == FieldKind::Message)
                {
                    auto it = plan.name_to_index.find(src.message_type);
                    if (it == plan.name_to_index.end()){
                        throw std::runtime_error(
                                "ProtoSchemeFlatMap::encode_into: nested type '" +
                                src.message_type + "' not in plan");
                    }

                    frec.nested_msg_idx = static_cast<int32_t>(it->second);
                }

                if (src.name.size() > 0xFFFF){
                    throw std::runtime_error("ProtoSchemeFlatMap: field name too long (>65535)");
                }

                frec.name_len = static_cast<uint16_t>(src.name.size());
                frec.name_offset = cur_name_off;

                write_bytes(
                        dst + fields_offset + (cur_field_idx + fi) * sizeof(FieldRecord),
                        &frec,
                        sizeof(frec));

                if (frec.name_len > 0){
                    write_bytes(dst + cur_name_off, src.name.data(), frec.name_len);
                }

                cur_name_off += frec.name_len;
            }
            cur_field_idx += mrec.fields_count;
        }
    }

    ProtoSchemeFlatMap::ProtoSchemeFlatMap(
            const std::string& proto_scheme_text,
            const std::string& root_message_name) : owned_(), view_()
    {
        ProtoSchemeMap built(proto_scheme_text);

        std::size_t size = compute_size(built, root_message_name);

        owned_.resize(size);

        encode_into(built, root_message_name, owned_.data(), owned_.size());

        view_ = std::span<const std::byte>(owned_.data(), owned_.size());

        validate();
    }

    ProtoSchemeFlatMap::ProtoSchemeFlatMap(std::span<const std::byte> bytes)
            : owned_(), view_(bytes)
    {
        validate();
    }

    const Header& ProtoSchemeFlatMap::header() const
    {
        return *reinterpret_cast<const Header*>(view_.data());
    }

    std::size_t ProtoSchemeFlatMap::message_count() const noexcept
    {
        if (view_.size() < sizeof(Header)){
            return 0;
        }
        return header().message_count;
    }

    uint32_t ProtoSchemeFlatMap::root_message_idx() const noexcept
    {
        return header().root_message_idx;
    }

    std::size_t ProtoSchemeFlatMap::field_count() const noexcept
    {
        if (view_.size() < sizeof(Header)){
            return 0;
        }
        return header().field_count;
    }

    const ProtoSchemeFlatMap::MessageRecord& ProtoSchemeFlatMap::message_at(std::size_t idx) const
    {
        const Header& h = header();

        if (idx >= h.message_count){
            throw std::out_of_range("ProtoSchemeFlatMap::message_at: out of range");
        }

        const auto* recs = reinterpret_cast<const MessageRecord*>(view_.data() + h.messages_offset);
        return recs[idx];
    }

    const ProtoSchemeFlatMap::FieldRecord* ProtoSchemeFlatMap::field_ptr(std::size_t idx) const
    {
        const Header& h = header();

        if (idx >= h.field_count){
            return nullptr;
        }

        const auto* recs = reinterpret_cast<const FieldRecord*>(view_.data() + h.fields_offset);

        return recs + idx;
    }

    const ProtoSchemeFlatMap::FieldRecord* ProtoSchemeFlatMap::find_field(
            const MessageRecord& msg,
            int field_number) const
    {
        const Header& h = header();
        const auto* base = reinterpret_cast<const FieldRecord*>(view_.data() + h.fields_offset);

        const FieldRecord* slice = base + msg.fields_start;

        for (uint32_t i = 0; i < msg.fields_count; ++i){
            if (slice[i].field_number == field_number){
                return slice + i;
            }
        }

        return nullptr;
    }

    std::string_view ProtoSchemeFlatMap::name_of(const MessageRecord& msg) const
    {
        if (static_cast<std::size_t>(msg.name_offset) + msg.name_len > view_.size()){
            throw std::runtime_error("ProtoSchemeFlatMap::name_of(msg): corrupted name region");
        }

        return std::string_view(
                reinterpret_cast<const char*>(view_.data() + msg.name_offset),
                msg.name_len);
    }

    std::string_view ProtoSchemeFlatMap::name_of(const FieldRecord& f) const
    {
        if (static_cast<std::size_t>(f.name_offset) + f.name_len > view_.size()){
            throw std::runtime_error("ProtoSchemeFlatMap::name_of(field): corrupted name region");
        }

        return std::string_view(
                reinterpret_cast<const char*>(view_.data() + f.name_offset),
                f.name_len);
    }

    void ProtoSchemeFlatMap::validate() const
    {
        if (view_.size() < sizeof(Header)){
            throw std::runtime_error("ProtoSchemeFlatMap: buffer smaller than header");
        }

        const Header& h = header();
        if (h.magic != MAGIC){
            throw std::runtime_error("ProtoSchemeFlatMap: bad magic");
        }
        if (h.version != VERSION){
            throw std::runtime_error("ProtoSchemeFlatMap: unsupported version");
        }

        const std::size_t messages_end =
                static_cast<std::size_t>(h.messages_offset) +
                static_cast<std::size_t>(h.message_count) * sizeof(MessageRecord);

        if (messages_end > view_.size() || h.messages_offset < sizeof(Header)){
            throw std::runtime_error("ProtoSchemeFlatMap: messages region invalid");
        }

        const std::size_t fields_end =
                static_cast<std::size_t>(h.fields_offset) +
                static_cast<std::size_t>(h.field_count) * sizeof(FieldRecord);
        if (h.fields_offset < messages_end || fields_end > view_.size()){
            throw std::runtime_error("ProtoSchemeFlatMap: fields region invalid");
        }

        if (h.names_offset < fields_end || h.names_offset > view_.size()){
            throw std::runtime_error("ProtoSchemeFlatMap: names region invalid");
        }

        if (h.root_message_idx >= h.message_count){
            throw std::runtime_error("ProtoSchemeFlatMap: root_message_idx out of range");
        }
    }
}