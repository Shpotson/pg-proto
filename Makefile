EXTENSION = pg_proto
MODULE_big = pg_proto

OBJS = \
	pg_proto.o \
	src/Proto.o \
	src/FlatProtoPath.o \
	src/ProtoSchemeMap.o \
	src/ProtoSchemeFlatMap.o \
	src/ProtoToJsonConverter.o \
	src/ProtoUtils.o

DATA = pg_proto--0.1.sql

REGRESS = basic \
          get_text_by_scheme_positive \
          get_int32_by_scheme_positive \
          get_int64_by_scheme_positive \
          get_float_by_scheme_positive \
          get_double_by_scheme_positive \
          get_int32_by_path_positive \
          get_int64_by_path_positive \
          get_text_by_path_positive \
          get_float_by_path_positive \
          get_double_by_path_positive \
          get_jsonb_by_scheme_positive \
          get_jsonb_by_scheme_map_positive \
          schema_registry_positive

REGRESS_OPTS = --inputdir=.

PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)

PGFILEDESC = "pg_proto - PostgreSQL extension for working with Protobuf: proto data type, on-the-fly deserialization of bytea payloads, and a schema registry for message schemas"

override PG_CPPFLAGS += -I$(CURDIR)/include
override PG_CXXFLAGS += -std=c++20 -Wall -Wextra -Wpedantic

ifeq ($(PORTNAME), win32)
SHLIB_LINK += -lws2_32
endif

UNIT_TEST_DIR = tests/unit
UNIT_TEST_BIN = $(UNIT_TEST_DIR)/unit_tests

UNIT_TEST_SRCS = \
	$(UNIT_TEST_DIR)/proto_tests.cpp \
	src/Proto.cpp \
	src/ProtoSchemeMap.cpp \
	src/ProtoSchemeFlatMap.cpp \
	src/FlatProtoPath.cpp \
	src/ProtoUtils.cpp

UNIT_TEST_CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic \
	-I$(CURDIR)/include \
	-I$(CURDIR)/$(UNIT_TEST_DIR)

unit-tests:
	$(CXX) $(UNIT_TEST_CXXFLAGS) $(UNIT_TEST_SRCS) -o $(UNIT_TEST_BIN)

run-unit-tests: unit-tests
	./$(UNIT_TEST_BIN)

clean-unit-tests:
	rm -f $(UNIT_TEST_BIN)

include $(PGXS)

override BITCODE_CXXFLAGS += -std=c++20