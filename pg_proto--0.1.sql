CREATE FUNCTION pg_proto_version()
    RETURNS text
AS 'MODULE_PATHNAME', 'pg_proto_version'
LANGUAGE C
STRICT;

COMMENT ON FUNCTION pg_proto_version() IS
'Returns pg_proto extension version';

CREATE FUNCTION get_text_by_scheme(scheme text, root_message text, command text, proto_data bytea)
    RETURNS text
AS 'MODULE_PATHNAME', 'get_text_by_scheme'
LANGUAGE C
STRICT;

CREATE FUNCTION get_int32_by_scheme(scheme text, root_message text, command text, proto_data bytea)
    RETURNS integer
AS 'MODULE_PATHNAME', 'get_int32_by_scheme'
LANGUAGE C
STRICT;

CREATE FUNCTION get_int64_by_scheme(scheme text, root_message text, command text, proto_data bytea)
    RETURNS bigint
AS 'MODULE_PATHNAME', 'get_int64_by_scheme'
LANGUAGE C
STRICT;

CREATE FUNCTION get_double_by_scheme(scheme text, root_message text, command text, proto_data bytea)
    RETURNS double precision
AS 'MODULE_PATHNAME', 'get_double_by_scheme'
LANGUAGE C
STRICT;

CREATE FUNCTION get_float_by_scheme(scheme text, root_message text, command text, proto_data bytea)
    RETURNS real
AS 'MODULE_PATHNAME', 'get_float_by_scheme'
LANGUAGE C
STRICT;