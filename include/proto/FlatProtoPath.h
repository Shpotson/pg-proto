#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>
#include <stdexcept>

#include "ProtoUtils.h"

namespace proto
{
    // ---------------------------------------------------------------------
    // Бинарный layout FlatProtoPath (он же payload bytea в Postgres).
    // Всё little-endian, упаковано без выравнивания > 4.
    //
    // Header (24 байта):
    //   uint32 magic        = 0x50504154 ('PPAT' little-endian)
    //   uint32 version      = 1
    //   uint32 step_count
    //   uint32 names_offset // смещение от начала payload до блока имён
    //   uint32 names_size
    //   uint32 reserved     // 0, выравнивание/расширение
    //
    // Steps[step_count] (по 12 байт):
    //   int32  field_number
    //   uint8  kind        // FieldKind, см. ProtoUtils.h
    //   uint8  reserved
    //   uint16 name_len
    //   uint32 name_offset // смещение от начала payload до имени
    //
    // NamesBlock: плотно упакованные байты имён, без NUL-терминаторов.
    // ---------------------------------------------------------------------

    namespace flat_proto_path_detail
    {
        constexpr uint32_t MAGIC   = 0x50504154u;
        constexpr uint32_t VERSION = 1u;

        struct Header {
            uint32_t magic;
            uint32_t version;
            uint32_t step_count;
            uint32_t names_offset;
            uint32_t names_size;
            uint32_t reserved;
        };
        static_assert(sizeof(Header) == 24, "FlatProtoPath::Header must be 24 bytes");

        struct StepRecord {
            int32_t  field_number;
            uint8_t  kind;
            uint8_t  reserved;
            uint16_t name_len;
            uint32_t name_offset;
        };
        static_assert(sizeof(StepRecord) == 12, "FlatProtoPath::StepRecord must be 12 bytes");
    }

    struct FlatPathStep {
        int       field_number;
        FieldKind kind;
    };


    class FlatProtoPath
    {
    public:
        explicit FlatProtoPath(std::span<const std::byte> bytes);

        explicit FlatProtoPath(const std::vector<PathStep>& steps);

        std::size_t size() const noexcept;
        bool empty() const noexcept { return size() == 0; }

        std::string_view name_of(std::size_t i) const;
        FlatPathStep operator[](std::size_t i) const;

        std::span<const std::byte> bytes() const noexcept { return view_; }
        const std::byte* data()  const noexcept { return view_.data(); }
        std::size_t byte_size() const noexcept { return view_.size(); }

        static std::size_t compute_size(const std::vector<PathStep>& steps) noexcept;
        static void encode_into(const std::vector<PathStep>& steps, std::byte* dst, std::size_t dst_size);

    private:
        std::vector<std::byte> owned_;
        std::span<const std::byte> view_;

        const flat_proto_path_detail::Header& header() const;
        void validate() const;
    };
}