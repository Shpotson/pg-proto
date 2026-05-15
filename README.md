# pg-proto
PostgreSQL extension for working with Protobuf: proto data type, on-the-fly deserialization of bytea payloads, and a schema registry for message schemas


source .venv_unix/bin/activate
export PGHOST=localhost
export PGPORT=5432
export PGPASSWORD=bench
export PGDATABASE=bench
python3 tests/benchmarks/bench_pg_proto.py