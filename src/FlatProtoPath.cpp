#include "proto/FlatProtoPath.h"

#include <cstring>
#include <stdexcept>

namespace proto
{
    using flat_proto_path_detail::Header;
    using flat_proto_path_detail::StepRecord;
    using flat_proto_path_detail::MAGIC;
    using flat_proto_path_detail::VERSION;

    static inline void write_bytes(std::byte* dst, const void* src, std::size_t n)
    {
        std::memcpy(dst, src, n);
    }

    std::size_t FlatProtoPath::compute_size(const std::vector<PathStep>& steps) noexcept
    {
        std::size_t total = sizeof(Header) + steps.size() * sizeof(StepRecord);

        for (const auto& s : steps){
            total += s.field_name.size();
        }

        return total;
    }

    void FlatProtoPath::encode_into(
            const std::vector<PathStep>& steps,
            std::byte* dst,
            std::size_t dst_size)
    {
        const std::size_t need = compute_size(steps);

        if (dst_size < need){
            throw std::runtime_error("FlatProtoPath::encode_into: dst buffer too small");
        }

        const uint32_t step_count   = static_cast<uint32_t>(steps.size());
        const uint32_t steps_offset = static_cast<uint32_t>(sizeof(Header));
        const uint32_t names_offset = steps_offset + step_count * sizeof(StepRecord);

        Header h{};

        h.magic = MAGIC;
        h.version = VERSION;
        h.step_count = step_count;
        h.names_offset = names_offset;
        h.names_size = static_cast<uint32_t>(need - names_offset);
        h.reserved = 0;
        write_bytes(dst, &h, sizeof(h));

        uint32_t cur_name_off = names_offset;
        for (uint32_t i = 0; i < step_count; ++i)
        {
            const PathStep& src = steps[i];

            StepRecord rec{};
            rec.field_number = src.field_number;
            rec.kind         = static_cast<uint8_t>(src.kind);
            rec.reserved     = 0;

            if (src.field_name.size() > 0xFFFF){
                throw std::runtime_error("FlatProtoPath: field_name too long (>65535)");
            }

            rec.name_len    = static_cast<uint16_t>(src.field_name.size());
            rec.name_offset = cur_name_off;

            write_bytes(
                    dst + steps_offset + i * sizeof(StepRecord),
                    &rec,
                    sizeof(rec));

            if (rec.name_len > 0){
                write_bytes(
                        dst + cur_name_off,
                        src.field_name.data(),
                        rec.name_len);
            }


            cur_name_off += rec.name_len;
        }
    }

    FlatProtoPath::FlatProtoPath(std::span<const std::byte> bytes) : owned_(), view_(bytes)
    {
        validate();
    }

    FlatProtoPath::FlatProtoPath(const std::vector<PathStep>& steps) : owned_(compute_size(steps)), view_()
    {
        encode_into(steps, owned_.data(), owned_.size());
        view_ = std::span<const std::byte>(owned_.data(), owned_.size());
        validate();
    }

    const Header& FlatProtoPath::header() const
    {
        return *reinterpret_cast<const Header*>(view_.data());
    }

    std::size_t FlatProtoPath::size() const noexcept
    {
        if (view_.size() < sizeof(Header)){
            return 0;
        }

        return header().step_count;
    }

    FlatPathStep FlatProtoPath::operator[](std::size_t i) const
    {
        const Header& h = header();
        if (i >= h.step_count){
            throw std::out_of_range("FlatProtoPath: step index out of range");
        }

        const StepRecord* recs = reinterpret_cast<const StepRecord*>(view_.data() + sizeof(Header));
        const StepRecord& rec = recs[i];

        FlatPathStep out;

        out.field_number = rec.field_number;
        out.kind = static_cast<FieldKind>(rec.kind);

        return out;
    }

    std::string_view FlatProtoPath::name_of(std::size_t i) const {
        const Header &h = header();
        if (i >= h.step_count){
            throw std::out_of_range("FlatProtoPath::name_of: step index out of range");
        }

        const StepRecord *recs = reinterpret_cast<const StepRecord *>(view_.data() + sizeof(Header));
        const StepRecord &rec = recs[i];

        if (static_cast<std::size_t>(rec.name_offset) + rec.name_len > view_.size()){
            throw std::runtime_error("FlatProtoPath::name_of: corrupted name region");
        }

        return {
                reinterpret_cast<const char *>(view_.data() + rec.name_offset),
                rec.name_len};
    }

    void FlatProtoPath::validate() const
    {
        if (view_.size() < sizeof(Header)){
            throw std::runtime_error("FlatProtoPath: buffer smaller than header");
        }

        const Header& h = header();

        if (h.magic != MAGIC){
            throw std::runtime_error("FlatProtoPath: bad magic");
        }

        if (h.version != VERSION){
            throw std::runtime_error("FlatProtoPath: unsupported version");
        }

        const std::size_t steps_end = sizeof(Header) + static_cast<std::size_t>(h.step_count) * sizeof(StepRecord);

        if (steps_end > view_.size()){
            throw std::runtime_error("FlatProtoPath: steps overflow buffer");
        }

        if (h.names_offset < steps_end || h.names_offset > view_.size()){
            throw std::runtime_error("FlatProtoPath: bad names_offset");
        }

        if (static_cast<std::size_t>(h.names_offset) + h.names_size > view_.size()){
            throw std::runtime_error("FlatProtoPath: names region overflows buffer");
        }
    }
}