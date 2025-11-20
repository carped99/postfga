-- test_queue.sql
-- SQL test script for queue functions

-- Create test functions
CREATE OR REPLACE FUNCTION postfga_test_enqueue(count int)
RETURNS int
AS 'MODULE_PATHNAME', 'postfga_test_enqueue'
LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION postfga_test_enqueue_continuous()
RETURNS int
AS 'MODULE_PATHNAME', 'postfga_test_enqueue_continuous'
LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION postfga_test_queue_stats()
RETURNS text
AS 'MODULE_PATHNAME', 'postfga_test_queue_stats'
LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION postfga_test_clear_queue()
RETURNS int
AS 'MODULE_PATHNAME', 'postfga_test_clear_queue'
LANGUAGE C STRICT;



-- Test 1: Check initial queue state
SELECT postfga_test_queue_stats() AS "Initial Queue State";

-- Test 2: Enqueue 5 test requests
SELECT postfga_test_enqueue(5) AS "Enqueued Count";

-- Test 3: Check queue state after enqueue
SELECT postfga_test_queue_stats() AS "Queue After Enqueue";

-- Test 4: Run continuous enqueue (1 per second for 10 seconds)
-- This will block for ~10 seconds
SELECT postfga_test_enqueue_continuous() AS "Continuous Enqueue Count";

-- Test 5: Check final queue state
SELECT postfga_test_queue_stats() AS "Final Queue State";

-- Test 6: Clear completed requests
SELECT postfga_test_clear_queue() AS "Cleared Count";

-- Test 7: Check queue state after clear
SELECT postfga_test_queue_stats() AS "Queue After Clear";
