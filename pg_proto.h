#ifndef PG_PROTO_H
#define PG_PROTO_H

#include "fmgr.h"

PGDLLEXPORT Datum pg_proto_version(PG_FUNCTION_ARGS);

PGDLLEXPORT Datum resolve_path(PG_FUNCTION_ARGS);

PGDLLEXPORT Datum get_text_by_scheme(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum get_int32_by_scheme(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum get_int64_by_scheme(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum get_float_by_scheme(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum get_double_by_scheme(PG_FUNCTION_ARGS);

PGDLLEXPORT Datum get_text_by_path(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum get_int32_by_path(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum get_int64_by_path(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum get_float_by_path(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum get_double_by_path(PG_FUNCTION_ARGS);

PGDLLEXPORT Datum resolve_scheme_map(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum get_jsonb_by_scheme(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum get_jsonb_by_scheme_map(PG_FUNCTION_ARGS);

#endif