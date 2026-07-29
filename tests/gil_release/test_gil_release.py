#!/usr/bin/env python3
"""release_gil(): C++ method bodies run without the GIL, so blocking native
calls from multiple Python threads overlap instead of serializing."""

import sys
import os
import time
import threading

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'build'))

import gil_worker

N_THREADS = 4
SLEEP_MS = 150

print("Test 1: blocking native calls overlap across threads...")
workers = [gil_worker.Worker() for _ in range(N_THREADS)]
threads = [threading.Thread(target=w.sleep_ms, args=(SLEEP_MS,)) for w in workers]

start = time.perf_counter()
for t in threads:
    t.start()
for t in threads:
    t.join()
elapsed_ms = (time.perf_counter() - start) * 1000

serial_ms = N_THREADS * SLEEP_MS
# With the GIL held for the sleep, elapsed would be ~600ms; released it's
# ~150ms. The 2x-serial/2 threshold leaves generous headroom for slow CI.
assert elapsed_ms < serial_ms / 2, (
    f"{N_THREADS} threads x {SLEEP_MS}ms took {elapsed_ms:.0f}ms — "
    f"native calls appear to serialize (GIL not released?)"
)
assert all(w.calls == 1 for w in workers)
print(f"  ✓ {N_THREADS} x {SLEEP_MS}ms sleeps finished in {elapsed_ms:.0f}ms (serial would be {serial_ms}ms)")

print("Test 2: results stay correct under concurrency...")
w = gil_worker.Worker()
expected = w.checksum(100_000)
results = []
lock = threading.Lock()

def crunch():
    r = gil_worker.Worker().checksum(100_000)
    with lock:
        results.append(r)

threads = [threading.Thread(target=crunch) for _ in range(N_THREADS)]
for t in threads:
    t.start()
for t in threads:
    t.join()
assert results == [expected] * N_THREADS, results
print(f"  ✓ {N_THREADS} concurrent checksums all equal {expected}")

print("Test 3: string round-trip still works with the policy enabled...")
assert w.greet("gil") == "hello, gil"
print("  ✓ conversion paths unaffected")

print("\nAll GIL-release tests passed!")
