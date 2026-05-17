-- 1) create initial v1 schema
SELECT create_new_proto_scheme(
               'Person.proto',
               $$
                   package tutorial;

message Person {
    string name = 1;
    int32 age = 2;
}
$$,
    'Person'
);

-- 2) check it landed
SELECT name, version, root_message FROM proto_schemas ORDER BY name, version;

-- 3) idempotent re-create
SELECT create_new_proto_scheme(
               'Person.proto',
               $$
                   package tutorial;

message Person {
    string name = 1;
    int32 age = 2;
}
$$,
    'Person'
);
SELECT count(*) FROM proto_schemas WHERE name = 'Person.proto';

-- 4) commit v2 with an added field
SELECT commit_new_version(
               'Person.proto',
               $$
                   package tutorial;

message Person {
    string name = 1;
    int32 age = 2;
    string email = 3;
}
$$
);

SELECT name, version, root_message FROM proto_schemas ORDER BY name, version;

-- 5) typical usage: resolve current scheme + root, then deserialize
--    proto bytes encode (name='Alice', age=30, email='alice@example.com') under v2
SELECT get_text_by_scheme(
               resolve_current_scheme('Person.proto'),
               resolve_scheme_root_message('Person.proto'),
               '%.name',
               '\x0a05416c696365101e1a11616c696365406578616d706c652e636f6d'::bytea
       );

SELECT get_int32_by_scheme(
               resolve_current_scheme('Person.proto'),
               resolve_scheme_root_message('Person.proto'),
               '%.age',
               '\x0a05416c696365101e1a11616c696365406578616d706c652e636f6d'::bytea
       );

SELECT get_text_by_scheme(
               resolve_current_scheme('Person.proto'),
               resolve_scheme_root_message('Person.proto'),
               '%.email',
               '\x0a05416c696365101e1a11616c696365406578616d706c652e636f6d'::bytea
       );

-- 6) commit_new_version on unknown scheme -> error
SELECT commit_new_version('Unknown', 'message X { string y = 1; }');