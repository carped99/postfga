SELECT fga_check(
           object_type,
           object_id,
           subject_type,
           subject_id,
           relation
       )
from fga_bench.tuples TABLESAMPLE BERNOULLI (0.1) LIMIT 100;