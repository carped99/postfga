# PostFGA Test Suite

## Test Programs

### 1. test_client.c

Standalone C program that tests the gRPC client (`client.cpp`).

**Features:**
- Sends async CHECK requests every second
- Sends sync CHECK request every 5 seconds
- Displays callback results
- Runs until Ctrl+C

**Build:**
```bash
cd /workspace/build
cmake -DBUILD_GRPC_TEST=ON ..
make test_grpc_client
```

**Run:**
```bash
./test_grpc_client [endpoint]
# Default endpoint: localhost:8080
```

**Example:**
```bash
./test_grpc_client dns:///localhost:8081
```

---

### 2. test_queue.c

PostgreSQL extension test functions for testing the request queue.

**Functions:**

1. `postfga_test_enqueue(count int)` - Enqueue multiple test requests at once
2. `postfga_test_enqueue_continuous()` - Enqueue 1 request per second for 10 seconds
3. `postfga_test_queue_stats()` - Display current queue statistics
4. `postfga_test_clear_queue()` - Clear completed requests

**Build:**

These functions are built into the main extension. Just rebuild and install:

```bash
cd /workspace/build
cmake --build .
cmake --install .
```

**Run:**

```sql
-- Restart PostgreSQL to reload extension
-- Then run:
\i /workspace/sql/test_queue.sql
```

**Manual Testing:**

```sql
-- Check queue state
SELECT postfga_test_queue_stats();

-- Enqueue 10 test requests
SELECT postfga_test_enqueue(10);

-- Check queue again
SELECT postfga_test_queue_stats();

-- Run continuous test (blocks for 10 seconds)
SELECT postfga_test_enqueue_continuous();

-- Clear completed requests
SELECT postfga_test_clear_queue();
```

---

## Expected Output

### test_client.c Output:

```
PostFGA Client Test
===================
Endpoint: localhost:8080
Press Ctrl+C to stop

Client initialized successfully
Client is healthy

[Request #1] Sending async check: document:budget#reader@user:alice
[Request #2] Sending async check: document:budget#reader@user:alice
[Callback #1] allowed=1, error_code=0, error_message=''
[Request #3] Sending async check: document:budget#reader@user:alice
[Callback #2] allowed=1, error_code=0, error_message=''
...
```

### test_queue.c Output:

```sql
-- Initial state
 Initial Queue State
----------------------
 Queue: 0/256 (0.0% full)

-- After enqueuing 5
 Enqueued Count
----------------
              5

 Queue After Enqueue
----------------------
 Queue: 5/256 (2.0% full)

-- After continuous enqueue (10 requests over 10 seconds)
 Continuous Enqueue Count
--------------------------
                       10

 Final Queue State
--------------------
 Queue: 15/256 (5.9% full)
```

---

## Troubleshooting

### test_client fails to connect

```
Error: Failed to initialize gRPC client
```

**Solution:** Make sure OpenFGA server is running on the specified endpoint.

```bash
docker run -p 8080:8080 -p 8081:8081 -p 3000:3000 openfga/openfga run
```

### test_queue functions not found

```
ERROR:  function postfga_test_enqueue(integer) does not exist
```

**Solution:** The test functions need to be built into the extension. Add to CMakeLists.txt:

```cmake
set(SOURCES
    ...
    tests/test_queue.c  # Add this line
)
```

Then rebuild and reinstall.

---

## Integration Test

Run both tests simultaneously to see full request flow:

**Terminal 1: Start OpenFGA**
```bash
docker run -p 8080:8080 -p 8081:8081 openfga/openfga run
```

**Terminal 2: Start PostgreSQL with extension**
```bash
psql -U postgres -c "SELECT postfga_test_enqueue_continuous();"
```

**Terminal 3: Monitor BGW logs**
```bash
tail -f /tmp/pgdata/logfile
```

You should see:
- Backend processes enqueuing requests
- BGW dequeuing and processing requests
- gRPC client sending checks to OpenFGA
- Callbacks updating request results
