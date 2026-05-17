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


CREATE DOMAIN ProtoPath AS bytea;

CREATE FUNCTION resolve_path(scheme text, root_message text, command text)
    RETURNS ProtoPath
AS 'MODULE_PATHNAME', 'resolve_path'
LANGUAGE C
STRICT IMMUTABLE;

COMMENT ON FUNCTION resolve_path(text, text, text) IS
'Resolves proto traversal path once and returns it as opaque bytea. Pass to get_*_by_path functions.';

CREATE FUNCTION get_text_by_path(proto_path ProtoPath, proto_data bytea)
    RETURNS text
AS 'MODULE_PATHNAME', 'get_text_by_path'
LANGUAGE C
STRICT;

CREATE FUNCTION get_int32_by_path(proto_path ProtoPath, proto_data bytea)
    RETURNS integer
AS 'MODULE_PATHNAME', 'get_int32_by_path'
LANGUAGE C
STRICT;

CREATE FUNCTION get_int64_by_path(proto_path ProtoPath, proto_data bytea)
    RETURNS bigint
AS 'MODULE_PATHNAME', 'get_int64_by_path'
LANGUAGE C
STRICT;

CREATE FUNCTION get_double_by_path(proto_path ProtoPath, proto_data bytea)
    RETURNS double precision
AS 'MODULE_PATHNAME', 'get_double_by_path'
LANGUAGE C
STRICT;

CREATE FUNCTION get_float_by_path(proto_path ProtoPath, proto_data bytea)
    RETURNS real
AS 'MODULE_PATHNAME', 'get_float_by_path'
LANGUAGE C
STRICT;

CREATE DOMAIN ProtoSchemeMap AS bytea;

CREATE FUNCTION resolve_scheme_map(scheme text, root_message text)
    RETURNS ProtoSchemeMap
AS 'MODULE_PATHNAME', 'resolve_scheme_map'
LANGUAGE C
STRICT IMMUTABLE;

COMMENT ON FUNCTION resolve_scheme_map(text, text) IS
'Builds a flat binary proto schema, rooted at given message, for use with get_jsonb_by_scheme_map.';

CREATE FUNCTION get_jsonb_by_scheme(scheme text, root_message text, proto_data bytea)
    RETURNS jsonb
AS 'MODULE_PATHNAME', 'get_jsonb_by_scheme'
LANGUAGE C
STRICT;

CREATE FUNCTION get_jsonb_by_scheme_map(scheme_map ProtoSchemeMap, proto_data bytea)
    RETURNS jsonb
AS 'MODULE_PATHNAME', 'get_jsonb_by_scheme_map'
LANGUAGE C
STRICT;

CREATE TABLE proto_schemas (
                               name          text   NOT NULL,
                               version       int    NOT NULL,
                               scheme        text   NOT NULL,
                               root_message  text   NOT NULL,
                               created_at    timestamptz NOT NULL DEFAULT now(),
                               PRIMARY KEY (name, version)
);

CREATE FUNCTION create_new_proto_scheme(p_name text, p_scheme text, p_root_message text)
    RETURNS void
AS $$
BEGIN
INSERT INTO proto_schemas (name, version, scheme, root_message)
VALUES (p_name, 1, p_scheme, p_root_message)
    ON CONFLICT (name, version) DO NOTHING;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION commit_new_version(p_name text, p_scheme text)
    RETURNS int
AS $$
DECLARE
last_version int;
    last_root    text;
BEGIN
SELECT ps.version, ps.root_message
INTO last_version, last_root
FROM proto_schemas ps
WHERE ps.name = p_name
ORDER BY ps.version DESC
    LIMIT 1;
IF last_version IS NULL THEN
        RAISE EXCEPTION 'proto scheme % does not exist', p_name;
END IF;
INSERT INTO proto_schemas (name, version, scheme, root_message)
VALUES (p_name, last_version + 1, p_scheme, last_root);
RETURN last_version + 1;
END;
$$ LANGUAGE plpgsql;


CREATE FUNCTION resolve_current_scheme(p_name text)
    RETURNS text
AS $$
SELECT ps.scheme
FROM proto_schemas ps
WHERE ps.name = p_name
ORDER BY ps.version DESC
    LIMIT 1;
$$ LANGUAGE sql STABLE;


CREATE FUNCTION resolve_scheme_root_message(p_name text)
    RETURNS text
AS $$
SELECT ps.root_message
FROM proto_schemas ps
WHERE ps.name = p_name
ORDER BY ps.version DESC
    LIMIT 1;
$$ LANGUAGE sql STABLE;