# Citadel interview – feed reconciler benchmark

This folder contains a single C++ program (`main.cpp`) that implements **five** different ways to reconcile three asynchronous execution feeds inside a fixed **time window**, and a simple benchmark that compares their throughput.

## Concept

Each feed emits `Execution` events:

- `id`: identifier used to correlate across feeds
- `price`, `size`: fields that should match across feeds for the same `id`
- `ts`: **arrival time** for that event on that feed (used instead of a separate `ts` argument)

The reconciler keeps per-`id` state for a **window** of time:

- **`onFeedOne/onFeedTwo/onFeedThree(exec)`**: ingest events; `exec.ts` is the arrival time
- **`onTimer(now)`**: expire old ids whose window has elapsed
- **`alert()`**: triggered when a mismatch exists for an `id` within the active window (in this repo the implementations just increment an alert counter used by the benchmark)

## Implementations

`main.cpp` includes five classes:

- **`ReconcilerVectorScan`**: sorted `std::vector` + `lower_bound`, expiry by full scan (`remove_if`)
- **`ReconcilerUnorderedScan`**: `std::unordered_map`, expiry by full scan
- **`ReconcilerUnorderedHeap`**: `std::unordered_map` + min-heap of expirations (avoids full scan)
- **`ReconcilerFlatHashHeap`**: open-addressing flat hash table + min-heap expirations
- **`ReconcilerDirectIndexWheel`**: direct indexing by `id` + timer wheel expirations (fastest when `id` range is bounded/dense)

## Build & run

From this directory:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/reconcile_bench
```

The program prints a small header describing the synthetic workload, then one line per implementation with **nanoseconds per event** and the number of alerts observed.

## Expiry policy in the benchmark

The benchmark calls `onTimer()` at **each window end** (tumbling windows): \(W, 2W, 3W, \dots\) where \(W = \)`window_width`.

Events are **sorted by `Execution.ts`** (with a random tie-break for equal timestamps). That matches treating `ts` as **arrival time**: the stream is processed in time order, while the three feeds can still be interleaved and jittered relative to each other. **Option A**: any event whose window has already ended relative to the last `onTimer(now)` is **dropped** (late relative to tumbling boundaries).

## Notes on “extreme speed”

- The fastest approach depends heavily on **how many distinct ids** are alive in the window and whether the **id space is bounded**.
- Full scans in `onTimer()` are typically the first thing to remove for high throughput; heap/wheel expiry avoids scanning all active ids.

