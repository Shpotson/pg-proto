#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ProtoUtils.h"
#include "ProtoSchemeMap.h"

namespace proto
{
    namespace proto_scheme_flat_map_detail
    {
        constexpr uint32_t MAGIC   = 0x50534D46u;
        constexpr uint32_t VERSION = 1u;

        struct Header {
            uint32_t magic;
            uint32_t version;
            uint32_t message_count;
            uint32_t messages_offset;
            uint32_t field_count;
            uint32_t fields_offset;
            uint32_t names_offset;
            uint32_t root_message_idx;
        };
        static_assert(sizeof(Header) == 32, "ProtoSchemeFlatMap::Header must be 32 bytes");

        struct MessageRecord {
            uint32_t fields_start;
            uint32_t fields_count;
            uint32_t name_offset;
            uint16_t name_len;
            uint16_t reserved;
        };
        static_assert(sizeof(MessageRecord) == 16, "ProtoSchemeFlatMap::MessageRecord must be 16 bytes");

        struct FieldRecord {
            int32_t  field_number;
            int32_t  nested_msg_idx;
            uint32_t name_offset;
            uint16_t name_len;
            uint8_t  kind;
            uint8_t  is_repeated;
        };
        static_assert(sizeof(FieldRecord) == 16, "ProtoSchemeFlatMap::FieldRecord must be 16 bytes");
    }

    class ProtoSchemeFlatMap
    {
    public:
        using FieldRecord   = proto_scheme_flat_map_detail::FieldRecord;
        using MessageRecord = proto_scheme_flat_map_detail::MessageRecord;

        ProtoSchemeFlatMap(const std::string& proto_scheme_text,
                           const std::string& root_message_name);

        explicit ProtoSchemeFlatMap(std::span<const std::byte> bytes);

        std::span<const std::byte> bytes() const noexcept { return view_; }
        const std::byte* data() const noexcept { return view_.data(); }
        std::size_t byte_size() const noexcept { return view_.size(); }

        std::size_t message_count() const noexcept;
        uint32_t    root_message_idx() const noexcept;

        const MessageRecord& message_at(std::size_t idx) const;

        const FieldRecord*   field_ptr(std::size_t idx) const;
        std::size_t          field_count() const noexcept;

        const FieldRecord* find_field(const MessageRecord& msg, int field_number) const;

        std::string_view name_of(const MessageRecord& msg) const;
        std::string_view name_of(const FieldRecord& f) const;

        static std::size_t compute_size(const ProtoSchemeMap& built_map,
                                        const std::string& root_message_name);

        static void encode_into(const ProtoSchemeMap& built_map,
                                const std::string& root_message_name,
                                std::byte* dst, std::size_t dst_size);

    private:
        std::vector<std::byte>      owned_;
        std::span<const std::byte>  view_;

        const proto_scheme_flat_map_detail::Header& header() const;
        void validate() const;
    };
}