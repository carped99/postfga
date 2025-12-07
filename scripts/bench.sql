CREATE SCHEMA IF NOT EXISTS postfga_bench;

-- 1) object_type 목록
CREATE TABLE IF NOT EXISTS postfga_bench.object_types (
    object_type text PRIMARY KEY
);

-- 2) subject_type 목록
CREATE TABLE IF NOT EXISTS postfga_bench.subject_types (
    subject_type text PRIMARY KEY
);

-- 3) 객체(type + id)
CREATE TABLE IF NOT EXISTS postfga_bench.objects (
    object_type text NOT NULL REFERENCES postfga_bench.object_types(object_type),
    object_id   text NOT NULL,
    PRIMARY KEY (object_type, object_id)
);

-- 4) 주체(type + id) : user, group, service 등
CREATE TABLE IF NOT EXISTS postfga_bench.subjects (
    subject_type text NOT NULL REFERENCES postfga_bench.subject_types(subject_type),
    subject_id   text NOT NULL,
    PRIMARY KEY (subject_type, subject_id)
);

CREATE TABLE IF NOT EXISTS postfga_bench.tuples (
    object_type   text NOT NULL,
    object_id     text NOT NULL,
    subject_type  text NOT NULL,
    subject_id    text NOT null,
    relation      text NOT NULL
);

INSERT INTO postfga_bench.object_types (object_type) VALUES
    ('doc'),
    ('folder'),
    ('image'),
    ('dataset'),
    ('project'),
    ('dashboard'),
    ('report'),
    ('notebook')
ON CONFLICT DO NOTHING;

INSERT INTO postfga_bench.subject_types (subject_type) VALUES
    ('user'),
    ('group'),
    ('service')
ON CONFLICT DO NOTHING;


INSERT INTO postfga_bench.subjects (subject_type, subject_id)
SELECT 'user', format('%s', g)
FROM generate_series(1, 10000) AS g
ON CONFLICT DO NOTHING;

INSERT INTO postfga_bench.subjects (subject_type, subject_id)
SELECT 'service', format('%s', g)
FROM generate_series(1, 100) AS g
ON CONFLICT DO NOTHING;

INSERT INTO postfga_bench.subjects (subject_type, subject_id)
SELECT 'group', format('%s', g)
FROM generate_series(1, 1000) AS g
ON CONFLICT DO NOTHING;

INSERT INTO postfga_bench.objects (object_type, object_id)
SELECT 'doc', format('%s', g)
FROM generate_series(1, 20000) AS g
ON CONFLICT DO NOTHING;

INSERT INTO postfga_bench.objects (object_type, object_id)
SELECT 'folder', format('%s', g)
FROM generate_series(1, 5000) AS g
ON CONFLICT DO NOTHING;


SELECT 'objects' AS what, count(*) FROM postfga_bench.objects
UNION ALL
SELECT 'subjects', count(*) FROM postfga_bench.subjects;

INSERT INTO postfga_bench.tuples (object_type, object_id, relation, subject_type, subject_id)
WITH rels AS (
    SELECT ARRAY['owner','viewer']::text[] AS rels
),
all_subjects AS (
    SELECT subject_type, subject_id
    FROM postfga_bench.subjects
)
SELECT
    o.object_type,
    o.object_id,
    rels.rels[1 + (random() * 1)::int] AS relation,
    s.subject_type,
    s.subject_id
FROM postfga_bench.objects o
CROSS JOIN LATERAL (
    -- object 하나당 랜덤 subject 10명
    SELECT subject_type, subject_id
    FROM all_subjects
    ORDER BY random()
    LIMIT 10
) s
CROSS JOIN rels;

truncate table postfga_bench.objects;
truncate table postfga_bench.subjects;

select postfga_write_tuple(t.object_type, t.object_id, t.subject_type, t.subject_id, t.relation) from postfga_bench.tuples t where t.subject_type = 'user';

select count(*) from postfga_bench.tuples;


select * from postfga_bench.subjects TABLESAMPLE BERNOULLI (5);

CREATE OR REPLACE FUNCTION postfga_seed_from_bench()
RETURNS void
LANGUAGE plpgsql
AS $$
DECLARE
    r record;
BEGIN
    FOR r IN
        SELECT object_type, object_id, subject_type, subject_id, relation
        FROM postfga_bench.tuples
    LOOP
        PERFORM postfga_write_tuple(
            r.object_type,
            r.object_id,
            r.subject_type,
            r.subject_id,
            r.relation
        );
    END LOOP;
END;
$$;

SELECT postfga_check(
           object_type,
           object_id,
           subject_type,
           subject_id,
           relation
       )
FROM postfga_bench.tuples
ORDER BY random()
LIMIT 10000;





alter system set shared_preload_libraries = 'postfga';
select pg_reload_conf();

create extension postfga;
drop extension postfga cascade;

alter system set postfga.endpoint = 'dns:///openfga:8081';
alter system set postfga.store_id = '01KBP4Z86PVX6PGKWYVX3SVJYM';


CREATE OR REPLACE VIEW postfga_bench.sample_tuples
AS SELECT object_type,
    object_id,
    subject_type,
    subject_id,
    relation
   FROM postfga_bench.tuples TABLESAMPLE system (0.05);