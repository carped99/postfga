CREATE SCHEMA IF NOT EXISTS fga_bench;

-- 1) object_type 목록
CREATE TABLE IF NOT EXISTS fga_bench.object_types (
    object_type text PRIMARY KEY
);

-- 2) subject_type 목록
CREATE TABLE IF NOT EXISTS fga_bench.subject_types (
    subject_type text PRIMARY KEY
);

-- 3) 객체(type + id)
CREATE TABLE IF NOT EXISTS fga_bench.objects (
    object_type text NOT NULL REFERENCES fga_bench.object_types(object_type),
    object_id   text NOT NULL,
    PRIMARY KEY (object_type, object_id)
);

-- 4) 주체(type + id) : user, group, service 등
CREATE TABLE IF NOT EXISTS fga_bench.subjects (
    subject_type text NOT NULL REFERENCES fga_bench.subject_types(subject_type),
    subject_id   text NOT NULL,
    PRIMARY KEY (subject_type, subject_id)
);

CREATE TABLE IF NOT EXISTS fga_bench.tuples (
    id            bigserial PRIMARY KEY,
    object_type   text NOT NULL,
    object_id     text NOT NULL,
    subject_type  text NOT NULL,
    subject_id    text NOT null,
    relation      text NOT NULL
);

INSERT INTO fga_bench.object_types (object_type) VALUES
    ('doc'),
    ('folder'),
    ('image'),
    ('dataset'),
    ('project'),
    ('dashboard'),
    ('report'),
    ('notebook')
ON CONFLICT DO NOTHING;

INSERT INTO fga_bench.subject_types (subject_type) VALUES
    ('user'),
    ('group')
ON CONFLICT DO NOTHING;

INSERT INTO fga_bench.subjects (subject_type, subject_id)
SELECT 'user', gen_random_uuid()
FROM generate_series(1, 10000) AS g
ON CONFLICT DO NOTHING;

INSERT INTO fga_bench.subjects (subject_type, subject_id)
SELECT 'group', gen_random_uuid()
FROM generate_series(1, 1000) AS g
ON CONFLICT DO NOTHING;

INSERT INTO fga_bench.objects (object_type, object_id)
SELECT 'doc', gen_random_uuid()
FROM generate_series(1, 20000) AS g
ON CONFLICT DO NOTHING;

INSERT INTO fga_bench.objects (object_type, object_id)
SELECT 'folder', gen_random_uuid()
FROM generate_series(1, 5000) AS g
ON CONFLICT DO NOTHING;


INSERT INTO fga_bench.tuples (object_type, object_id, relation, subject_type, subject_id)
WITH rels AS (
    SELECT ARRAY['owner','viewer']::text[] AS rels
),
all_subjects AS (
    SELECT subject_type, subject_id
    FROM fga_bench.subjects
)
SELECT
    o.object_type,
    o.object_id,
    rels.rels[1 + (random() * 1)::int] AS relation,
    s.subject_type,
    s.subject_id
FROM fga_bench.objects o
CROSS JOIN LATERAL (
    -- object 하나당 랜덤 subject 10명
    SELECT subject_type, subject_id
    FROM all_subjects
    ORDER BY random()
    LIMIT 10
) s
CROSS JOIN rels;


-- 벤치마크 클라이언트별로 나누어 삽입 테스트
-- fga_write.sql
-- Usage:
pgbench -U postgres -d postgres -c 12 -j 12 -t 1 -n -f fga_write.sql

pgbench -U postgres -d postgres -c 30 -j 16 -T 30 -P 1 -n -f fga_check.sql

pgbench -U postgres -d postgres -c 60 -j 16 -T 30 -P 1 -n -f fga_check.sql

pgbench -U postgres -d postgres -c 200 -j 16 -T 30 -P 1 -n -f fga_check.sql

pgbench -U postgres -d postgres -c 900 -j 16 -T 300 -P 1 -n -f fga_check.sql
