\set nclients 10

SELECT fga_write_tuple(
         t.object_type,
         t.object_id,
         t.subject_type,
         t.subject_id,
         t.relation
       )
FROM fga_bench.tuples AS t
WHERE (t.id % :nclients) = :client_id;
