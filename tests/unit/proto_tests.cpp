#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_NO_POSIX_SIGNALS
#include "catch.hpp"

#include <span>

#include "../../include/proto/ProtoSchemeMap.h"
#include "../../include/proto/Proto.h"

#include <span>

using namespace proto;

const std::string SCHEMA_DEPTH_2 = R"(
    package tutorial;

    message Person {
        string name = 1;
        int32 id = 2;
        string email = 3;
        PhoneNumber phone = 4;
    }

    message PhoneNumber {
        string number = 1;
    }
)";

// ─── Бинарные данные ─────────────────────────
// name  = "Alice"            (field 1, string)
// id    = 42                 (field 2, int32)
// email = "alice@example.com"(field 3, string)
// phone.number = "+71234567890" (field 4 → field 1, string)
constexpr std::byte PERSON_PROTO[] = {
        std::byte{0x0a}, std::byte{0x05}, std::byte{0x41}, std::byte{0x6c},
        std::byte{0x69}, std::byte{0x63}, std::byte{0x65}, std::byte{0x10},
        std::byte{0x2a}, std::byte{0x1a}, std::byte{0x11}, std::byte{0x61},
        std::byte{0x6c}, std::byte{0x69}, std::byte{0x63}, std::byte{0x65},
        std::byte{0x40}, std::byte{0x65}, std::byte{0x78}, std::byte{0x61},
        std::byte{0x6d}, std::byte{0x70}, std::byte{0x6c}, std::byte{0x65},
        std::byte{0x2e}, std::byte{0x63}, std::byte{0x6f}, std::byte{0x6d},
        std::byte{0x22}, std::byte{0x0e}, std::byte{0x0a}, std::byte{0x0c},
        std::byte{0x2b}, std::byte{0x37}, std::byte{0x31}, std::byte{0x32},
        std::byte{0x33}, std::byte{0x34}, std::byte{0x35}, std::byte{0x36},
        std::byte{0x37}, std::byte{0x38}, std::byte{0x39}, std::byte{0x30}
}; // 44 bytes

TEST_CASE("simple smoke test", "[unit]") {
    REQUIRE(1 == 1);
}

TEST_CASE("two-level path: phone.number", "[unit]") {
    ProtoSchemeMap schema(SCHEMA_DEPTH_2);
    auto path = schema.resolve_path("Person", "%.phone.number");

    REQUIRE(path.size() == 2);

    REQUIRE(path[0].field_name         == "phone");
    REQUIRE(path[0].field_number       == 4);
    REQUIRE(path[0].kind               == FieldKind::Message);
    REQUIRE(path[0].nested_message_type == "PhoneNumber");

    REQUIRE(path[1].field_name         == "number");
    REQUIRE(path[1].field_number       == 1);
    REQUIRE(path[1].kind               == FieldKind::String);
    REQUIRE(path[1].nested_message_type == "");
}

TEST_CASE("scalar field first level: id", "[unit]") {
    ProtoSchemeMap schema(SCHEMA_DEPTH_2);
    auto path = schema.resolve_path("Person", "%.id");

    REQUIRE(path.size() == 1);
    REQUIRE(path[0].field_name   == "id");
    REQUIRE(path[0].field_number == 2);
    REQUIRE(path[0].kind         == FieldKind::Int32);
}

TEST_CASE("path without %.-prefix", "[unit]") {
    ProtoSchemeMap schema(SCHEMA_DEPTH_2);
    auto path = schema.resolve_path("Person", "name");

    REQUIRE(path.size() == 1);
    REQUIRE(path[0].field_name   == "name");
    REQUIRE(path[0].field_number == 1);
    REQUIRE(path[0].kind         == FieldKind::String);
}

TEST_CASE("unknown field throws", "[unit]") {
    ProtoSchemeMap schema(SCHEMA_DEPTH_2);
    REQUIRE_THROWS_AS(
            schema.resolve_path("Person", "%.unknown"),
            std::runtime_error
    );
}

TEST_CASE("unknown root message throws", "[unit]") {
    ProtoSchemeMap schema(SCHEMA_DEPTH_2);
    REQUIRE_THROWS_AS(
            schema.resolve_path("NoSuchMessage", "%.id"),
            std::runtime_error
    );
}

TEST_CASE("e2e: resolve string field 'name'", "[e2e]") {
    ProtoSchemeMap schema(SCHEMA_DEPTH_2);
    Proto proto(std::span<const std::byte>(PERSON_PROTO, sizeof(PERSON_PROTO)));

    auto path   = schema.resolve_path("Person", "%.name");
    auto field  = proto.resolve_field_by_path(path);

    REQUIRE(field.field_name == "name");
    REQUIRE(field.kind       == FieldKind::String);
    REQUIRE(std::get<std::string>(field.value) == "Alice");
}

TEST_CASE("e2e: resolve int32 field 'id'", "[e2e]") {
    ProtoSchemeMap schema(SCHEMA_DEPTH_2);
    Proto proto(std::span<const std::byte>(PERSON_PROTO, sizeof(PERSON_PROTO)));

    auto path   = schema.resolve_path("Person", "%.id");
    auto field  = proto.resolve_field_by_path(path);

    REQUIRE(field.field_name == "id");
    REQUIRE(field.kind       == FieldKind::Int32);
    REQUIRE(std::get<int32_t>(field.value) == 42);
}

TEST_CASE("e2e: resolve string field 'email'", "[e2e]") {
    ProtoSchemeMap schema(SCHEMA_DEPTH_2);
    Proto proto(std::span<const std::byte>(PERSON_PROTO, sizeof(PERSON_PROTO)));

    auto path   = schema.resolve_path("Person", "%.email");
    auto field  = proto.resolve_field_by_path(path);

    REQUIRE(field.kind == FieldKind::String);
    REQUIRE(std::get<std::string>(field.value) == "alice@example.com");
}

TEST_CASE("e2e: resolve nested field 'phone.number'", "[e2e]") {
    ProtoSchemeMap schema(SCHEMA_DEPTH_2);
    Proto proto(std::span<const std::byte>(PERSON_PROTO, sizeof(PERSON_PROTO)));

    auto path   = schema.resolve_path("Person", "%.phone.number");
    auto field  = proto.resolve_field_by_path(path);

    REQUIRE(field.field_name == "number");
    REQUIRE(field.kind       == FieldKind::String);
    REQUIRE(std::get<std::string>(field.value) == "+71234567890");
}

TEST_CASE("e2e: missing field in binary throws", "[e2e]") {
    // Создаём Proto из пустого буфера
    Proto proto(std::span<const std::byte>{});

    ProtoSchemeMap schema(SCHEMA_DEPTH_2);
    auto path = schema.resolve_path("Person", "%.id");

    REQUIRE_THROWS_AS(proto.resolve_field_by_path(path), std::runtime_error);
}