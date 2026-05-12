

import os
import random
import string
import statistics
import struct
import time
from io import StringIO

import psycopg
from psycopg import sql

N = 10000
WARMUP_ROUNDS = 2
MEASURE_ROUNDS = 5
REPORT_PATH = "bench_results.txt"

CONN_KWARGS = dict(
    host=os.environ.get("PGHOST",     "localhost"),
    port=int(os.environ.get("PGPORT", 5432)),
    dbname=os.environ.get("PGDATABASE", "postgres"),
    user=os.environ.get("PGUSER",     "postgres"),
    password=os.environ.get("PGPASSWORD", "postgres"),
)

FIRST_NAMES = [
    "Alice", "Bob", "Charlie", "Diana", "Egor", "Fyodor", "Galina",
    "Hiroshi", "Ivan", "Julia", "Kirill", "Lena", "Mikhail", "Nina",
    "Olga", "Petr", "Quentin", "Roman", "Sergey", "Tatiana",
]
CITIES = [
    "Moscow", "New York", "Berlin", "Tokyo", "Paris", "London",
    "Saint Petersburg", "Beijing", "Madrid", "Rome",
]
STREETS = [
    "Tverskaya", "Nevsky", "Arbat", "Broadway", "Unter den Linden",
    "Champs-Elysees", "Abbey Road", "Wall Street",
]
EMAIL_DOMAINS = ["example.com", "mail.ru", "gmail.com", "yahoo.com", "proton.me"]

WT_VARINT = 0
WT_64BIT  = 1
WT_LEN    = 2
WT_32BIT  = 5

def _varint(n: int) -> bytes:
    if n < 0:
        n &= (1 << 64) - 1
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        if n:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def _tag(field_num: int, wire_type: int) -> bytes:
    return _varint((field_num << 3) | wire_type)


def _zz32(v: int) -> int:
    return ((v << 1) ^ (v >> 31)) & 0xFFFFFFFF


def _zz64(v: int) -> int:
    return ((v << 1) ^ (v >> 63)) & 0xFFFFFFFFFFFFFFFF


def enc_string(field_num: int, s: str) -> bytes:
    payload = s.encode("utf-8")
    return _tag(field_num, WT_LEN) + _varint(len(payload)) + payload


def enc_message(field_num: int, body: bytes) -> bytes:
    return _tag(field_num, WT_LEN) + _varint(len(body)) + body


def enc_int32(field_num: int, v: int) -> bytes:
    return _tag(field_num, WT_VARINT) + _varint(v)


def enc_int64(field_num: int, v: int) -> bytes:
    return _tag(field_num, WT_VARINT) + _varint(v)


def enc_uint32(field_num: int, v: int) -> bytes:
    return _tag(field_num, WT_VARINT) + _varint(v)


def enc_sint32(field_num: int, v: int) -> bytes:
    return _tag(field_num, WT_VARINT) + _varint(_zz32(v))


def enc_sint64(field_num: int, v: int) -> bytes:
    return _tag(field_num, WT_VARINT) + _varint(_zz64(v))


def enc_bool(field_num: int, v: bool) -> bytes:
    return _tag(field_num, WT_VARINT) + _varint(1 if v else 0)


def enc_float(field_num: int, v: float) -> bytes:
    return _tag(field_num, WT_32BIT) + struct.pack("<f", v)


def enc_double(field_num: int, v: float) -> bytes:
    return _tag(field_num, WT_64BIT) + struct.pack("<d", v)


def build_address(addr: dict) -> bytes:
    return (
        enc_string(1, addr["street"])
        + enc_string(2, addr["city"])
        + enc_uint32(3, addr["zip"])
        + enc_int64(4, addr["building_id"])
        + enc_double(5, addr["latitude"])
        + enc_float(6, addr["longitude"])
        + enc_bool(7, addr["is_primary"])
    )


def build_phone(phone: dict) -> bytes:
    return enc_string(1, phone["number"])


def build_person(p: dict) -> bytes:
    return (
        enc_string(1, p["name"])
        + enc_int32(2, p["id"])
        + enc_string(3, p["email"])
        + enc_message(4, build_phone(p["phone"]))
        + enc_sint64(5, p["balance"])
        + enc_double(6, p["height_m"])
        + enc_float(7, p["weight_kg"])
        + enc_bool(8, p["is_active"])
        + enc_message(9, build_address(p["address"]))
        + enc_sint32(10, p["score"])
    )

def gen_person(idx: int) -> dict:
    rng = random.Random(idx)
    name = rng.choice(FIRST_NAMES)
    email_local = name.lower() + str(rng.randint(1, 9999))
    return {
        "name": name,
        "id": idx,
        "email": f"{email_local}@{rng.choice(EMAIL_DOMAINS)}",
        "phone": {"number": "+7" + "".join(rng.choices(string.digits, k=10))},
        "balance": rng.randint(-10**10, 10**10),
        "height_m": round(rng.uniform(1.50, 2.10), 4),
        "weight_kg": round(rng.uniform(45.0, 120.0), 2),
        "is_active": rng.random() < 0.7,
        "address": {
            "street": rng.choice(STREETS) + " " + str(rng.randint(1, 200)),
            "city": rng.choice(CITIES),
            "zip": rng.randint(10000, 999999),
            "building_id": rng.randint(10**9, 10**12),
            "latitude": round(rng.uniform(-90.0, 90.0), 6),
            "longitude": round(rng.uniform(-180.0, 180.0), 6),
            "is_primary": rng.random() < 0.5,
        },
        "score": rng.randint(-1000, 1000),
    }

PROTO_SCHEMA = r"""package tutorial;

message Person {
    string name = 1;
    int32 id = 2;
    string email = 3;
    PhoneNumber phone = 4;
    sint64 balance = 5;
    double height_m = 6;
    float weight_kg = 7;
    bool is_active = 8;
    Address address = 9;
    sint32 score = 10;
}

message PhoneNumber {
    string number = 1;
}

message Address {
    string street = 1;
    string city = 2;
    uint32 zip = 3;
    int64 building_id = 4;
    double latitude = 5;
    float longitude = 6;
    bool is_primary = 7;
}
"""


# ---------------------------------------------------------------------------
# Schema setup / teardown
# ---------------------------------------------------------------------------

DDL = """
DROP EXTENSION IF EXISTS pg_proto;
CREATE EXTENSION pg_proto;

DROP TABLE IF EXISTS bench_json;
DROP TABLE IF EXISTS bench_jsonb;
DROP TABLE IF EXISTS bench_proto;

CREATE TABLE bench_json  (id bigint PRIMARY KEY, data json);
CREATE TABLE bench_jsonb (id bigint PRIMARY KEY, data jsonb);
CREATE TABLE bench_proto (id bigint PRIMARY KEY, data bytea);
"""


def setup(conn):
    with conn.cursor() as cur:
        for stmt in [s for s in DDL.split(";") if s.strip()]:
            cur.execute(stmt)
    conn.commit()


def teardown(conn):
    with conn.cursor() as cur:
        cur.execute("DROP TABLE IF EXISTS bench_json;")
        cur.execute("DROP TABLE IF EXISTS bench_jsonb;")
        cur.execute("DROP TABLE IF EXISTS bench_proto;")
        cur.execute("DROP EXTENSION IF EXISTS pg_proto;")
    conn.commit()

def _pg_bytea_literal_for_copy(b: bytes) -> str:
    return "\\\\x" + b.hex()


def insert_all(conn, persons_with_proto):
    import json as _json

    # JSON
    buf = StringIO()
    for pid, p, _ in persons_with_proto:
        s = _json.dumps(p, ensure_ascii=False, separators=(",", ":"))
        s = (s.replace("\\", "\\\\")
               .replace("\t", "\\t")
               .replace("\n", "\\n")
               .replace("\r", "\\r"))
        buf.write(f"{pid}\t{s}\n")
    buf.seek(0)
    with conn.cursor() as cur, cur.copy("COPY bench_json (id, data) FROM STDIN") as cp:
        cp.write(buf.getvalue())

    # JSONB — то же самое
    buf = StringIO()
    for pid, p, _ in persons_with_proto:
        s = _json.dumps(p, ensure_ascii=False, separators=(",", ":"))
        s = (s.replace("\\", "\\\\")
               .replace("\t", "\\t")
               .replace("\n", "\\n")
               .replace("\r", "\\r"))
        buf.write(f"{pid}\t{s}\n")
    buf.seek(0)
    with conn.cursor() as cur, cur.copy("COPY bench_jsonb (id, data) FROM STDIN") as cp:
        cp.write(buf.getvalue())

    # PROTO (bytea hex)
    buf = StringIO()
    for pid, _, b in persons_with_proto:
        buf.write(f"{pid}\t{_pg_bytea_literal_for_copy(b)}\n")
    buf.seek(0)
    with conn.cursor() as cur, cur.copy("COPY bench_proto (id, data) FROM STDIN") as cp:
        cp.write(buf.getvalue())

    conn.commit()


QUERIES = {
    # ---- JSON ----
    "json   / Q1 city='Moscow' count":
        "SELECT count(*) FROM bench_json "
        "WHERE data->'address'->>'city' = 'Moscow'",
    "json   / Q2 sum(address.zip)":
        "SELECT sum((data->'address'->>'zip')::bigint) FROM bench_json",
    "json   / Q3 sum(address.building_id)":
        "SELECT sum((data->'address'->>'building_id')::bigint) FROM bench_json",
    "json   / Q4 sum(score)":
        "SELECT sum((data->>'score')::bigint) FROM bench_json",

    # ---- JSONB ----
    "jsonb  / Q1 city='Moscow' count":
        "SELECT count(*) FROM bench_jsonb "
        "WHERE data->'address'->>'city' = 'Moscow'",
    "jsonb  / Q2 sum(address.zip)":
        "SELECT sum((data->'address'->>'zip')::bigint) FROM bench_jsonb",
    "jsonb  / Q3 sum(address.building_id)":
        "SELECT sum((data->'address'->>'building_id')::bigint) FROM bench_jsonb",
    "jsonb  / Q4 sum(score)":
        "SELECT sum((data->>'score')::bigint) FROM bench_jsonb",

    # ---- PROTO via _by_scheme (resolve на каждой строке) ----
    "proto  / Q1 city='Moscow' count   (_by_scheme)":
        "SELECT count(*) FROM bench_proto "
        "WHERE get_text_by_scheme(%(schema)s, 'Person', '%%.address.city', data) = 'Moscow'",
    "proto  / Q2 sum(address.zip)      (_by_scheme)":
        "SELECT sum(get_int32_by_scheme(%(schema)s, 'Person', '%%.address.zip', data)::bigint) "
        "FROM bench_proto",
    "proto  / Q3 sum(address.building_id) (_by_scheme)":
        "SELECT sum(get_int64_by_scheme(%(schema)s, 'Person', '%%.address.building_id', data)) "
        "FROM bench_proto",
    "proto  / Q4 sum(score)             (_by_scheme)":
        "SELECT sum(get_int32_by_scheme(%(schema)s, 'Person', '%%.score', data)::bigint) "
        "FROM bench_proto",

    # ---- PROTO via _by_path (resolve один раз через CTE) ----
    "proto  / Q1 city='Moscow' count   (_by_path)":
        "WITH p AS (SELECT resolve_path(%(schema)s, 'Person', '%%.address.city') AS path) "
        "SELECT count(*) FROM bench_proto, p "
        "WHERE get_text_by_path(p.path, bench_proto.data) = 'Moscow'",
    "proto  / Q2 sum(address.zip)      (_by_path)":
        "WITH p AS (SELECT resolve_path(%(schema)s, 'Person', '%%.address.zip') AS path) "
        "SELECT sum(get_int32_by_path(p.path, bench_proto.data)::bigint) "
        "FROM bench_proto, p",
    "proto  / Q3 sum(address.building_id) (_by_path)":
        "WITH p AS (SELECT resolve_path(%(schema)s, 'Person', '%%.address.building_id') AS path) "
        "SELECT sum(get_int64_by_path(p.path, bench_proto.data)) "
        "FROM bench_proto, p",
    "proto  / Q4 sum(score)             (_by_path)":
        "WITH p AS (SELECT resolve_path(%(schema)s, 'Person', '%%.score') AS path) "
        "SELECT sum(get_int32_by_path(p.path, bench_proto.data)::bigint) "
        "FROM bench_proto, p",
}

def _fmt_bytes(n: int) -> str:
    if n < 1024:                return f"{n} B"
    if n < 1024 * 1024:         return f"{n / 1024:.1f} KiB"
    if n < 1024 * 1024 * 1024:  return f"{n / (1024 * 1024):.2f} MiB"
    return f"{n / (1024 * 1024 * 1024):.2f} GiB"

SIZE_SQL = """
           SELECT
               pg_total_relation_size(c.oid)                                        AS total_bytes,
               pg_relation_size(c.oid, 'main')                                      AS heap_bytes,
               COALESCE(pg_total_relation_size(t.oid), 0)                           AS toast_bytes,
               pg_indexes_size(c.oid)                                               AS index_bytes
           FROM pg_class c
                    LEFT JOIN pg_class t ON t.oid = c.reltoastrelid
           WHERE c.relname = %s \
           """

AVG_COLUMN_SQL = "SELECT avg(pg_column_size(data))::bigint FROM {table}"


def collect_sizes(conn) -> dict:
    """Считаем реальные размеры всех трёх табличек после ANALYZE."""
    out = {}
    with conn.cursor() as cur:
        for table in ("bench_json", "bench_jsonb", "bench_proto"):
            cur.execute(SIZE_SQL, (table,))
            total, heap, toast, idx = cur.fetchone()
            cur.execute(AVG_COLUMN_SQL.format(table=table))
            avg_col = cur.fetchone()[0] or 0
            out[table] = {
                "total":    int(total),
                "heap":     int(heap),
                "toast":    int(toast),
                "indexes":  int(idx),
                "avg_col":  int(avg_col),
            }
    return out

def print_sizes(sizes: dict) -> str:
    """Формирует текстовый блок для отчёта."""
    lines = []
    lines.append(f"{'table':12s}  {'total':>11s}  {'heap':>11s}  "
                 f"{'toast':>11s}  {'indexes':>11s}  {'avg col':>10s}")
    lines.append("-" * 78)
    base_total = sizes["bench_proto"]["total"]
    base_col   = sizes["bench_proto"]["avg_col"]
    for table in ("bench_json", "bench_jsonb", "bench_proto"):
        s = sizes[table]
        lines.append(
            f"{table:12s}  "
            f"{_fmt_bytes(s['total']):>11s}  "
            f"{_fmt_bytes(s['heap']):>11s}  "
            f"{_fmt_bytes(s['toast']):>11s}  "
            f"{_fmt_bytes(s['indexes']):>11s}  "
            f"{_fmt_bytes(s['avg_col']):>10s}"
        )
    lines.append("")
    lines.append("Ratios (vs bench_proto):")
    for table in ("bench_json", "bench_jsonb", "bench_proto"):
        s = sizes[table]
        r_total = s["total"]   / base_total if base_total else 0
        r_col   = s["avg_col"] / base_col   if base_col   else 0
        lines.append(f"  {table:12s}  total ×{r_total:5.2f}   avg col ×{r_col:5.2f}")
    return "\n".join(lines)

def time_query(conn, sql_text: str, params: dict, rounds: int) -> list:
    timings = []
    with conn.cursor() as cur:
        for _ in range(rounds):
            t0 = time.perf_counter()
            cur.execute(sql_text, params)
            _ = cur.fetchone()
            timings.append(time.perf_counter() - t0)
    return timings


def warmup_all(conn, params):
    with conn.cursor() as cur:
        cur.execute("SELECT count(*) FROM bench_json")
        cur.fetchone()
        cur.execute("SELECT count(*) FROM bench_jsonb")
        cur.fetchone()
        cur.execute("SELECT count(*) FROM bench_proto")
        cur.fetchone()
    # дополнительно один проход каждого запроса
    for label, q in QUERIES.items():
        time_query(conn, q, params, WARMUP_ROUNDS)

def main():
    print(f"Generating {N} rows ...")
    data = []
    for i in range(1, N + 1):
        p = gen_person(i)
        b = build_person(p)
        data.append((i, p, b))

    avg_proto_size = statistics.mean(len(b) for _, _, b in data)
    avg_json_size  = statistics.mean(
        len(__import__("json").dumps(p, ensure_ascii=False)) for _, p, _ in data
    )
    print(f"  avg proto bytes:  {avg_proto_size:.1f}")
    print(f"  avg json  chars:  {avg_json_size:.1f}")

    print("Connecting:", {k: v for k, v in CONN_KWARGS.items() if k != "password"})
    with psycopg.connect(**CONN_KWARGS) as conn:
        conn.autocommit = False

        print("Setup (extension + tables) ...")
        setup(conn)

        print("Inserting via COPY ...")
        t0 = time.perf_counter()
        insert_all(conn, data)
        print(f"  inserted in {time.perf_counter() - t0:.2f}s")

        with conn.cursor() as cur:
            cur.execute("ANALYZE bench_json;")
            cur.execute("ANALYZE bench_jsonb;")
            cur.execute("ANALYZE bench_proto;")
        conn.commit()

        print("Table sizes:")
        sizes = collect_sizes(conn)
        sizes_block = print_sizes(sizes)
        print(sizes_block)

        params = {"schema": PROTO_SCHEMA}

        print(f"Warm-up ({WARMUP_ROUNDS} rounds per query) ...")
        warmup_all(conn, params)

        print(f"Measuring ({MEASURE_ROUNDS} rounds per query) ...")
        results = {}
        for label, q in QUERIES.items():
            ts = time_query(conn, q, params, MEASURE_ROUNDS)
            results[label] = ts
            print(f"  {label:55s}  min={min(ts)*1000:8.2f} ms  "
                  f"med={statistics.median(ts)*1000:8.2f} ms")

        print("Teardown ...")
        teardown(conn)

    with open(REPORT_PATH, "w", encoding="utf-8") as f:
        f.write(f"pg_proto stress benchmark\n")
        f.write(f"N = {N}\n")
        f.write(f"warmup_rounds = {WARMUP_ROUNDS}, measure_rounds = {MEASURE_ROUNDS}\n")
        f.write(f"avg proto bytes: {avg_proto_size:.1f}\n")
        f.write(f"avg json  chars: {avg_json_size:.1f}\n")
        f.write("\n")
        f.write(f"{'query':60s}  {'min ms':>10s}  {'med ms':>10s}  "
                f"{'mean ms':>10s}  {'max ms':>10s}\n")
        f.write("-" * 110 + "\n")
        for label, ts in results.items():
            f.write(f"{label:60s}  "
                    f"{min(ts)*1000:10.2f}  "
                    f"{statistics.median(ts)*1000:10.2f}  "
                    f"{statistics.mean(ts)*1000:10.2f}  "
                    f"{max(ts)*1000:10.2f}\n")
    print(f"\nReport written to {REPORT_PATH}")


if __name__ == "__main__":
    main()
