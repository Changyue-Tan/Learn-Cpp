# Benchmark methodology (practical)

These demos are small, so it’s easy to get misleading numbers. Use this as a checklist for **repeatable** results.

## Build matters

- Use **Release** (this repo defaults to Release for single-config generators).\n- Keep the build directory consistent between runs.

## Reduce noise (minimum viable)

- Close background load (browser, updates, indexing).\n- Run each benchmark multiple times (`--repeat` in the harness) and look for stability.\n- Prefer relative comparisons (A vs B on the same machine) over absolute “my ops/sec”.

## CPU pinning (when you care)

If you are comparing two designs, pin threads to CPUs to reduce scheduling variance.\n\nExamples (Linux):\n\n```bash\ntaskset -c 2 ./out/build/ninja/lockfree_spsc_ring_buffer\n```\n\nFor tighter control, also consider:\n\n- `isolcpus` kernel args (advanced)\n- `chrt` real-time scheduling (advanced; can be dangerous on your workstation)

## Warmup and caches

- The first run is often slower due to instruction cache and page faults.\n- If you add your own benchmarks, include a short warmup loop before timing.

## Throughput vs tail latency

- **Throughput** (ops/sec) answers “how much work per second”.\n- **Tail latency** (p99/p99.9) answers “how bad are the worst cases”.\n\nMost HFT-style problems care about tail latency under load; throughput-only numbers can hide queueing and contention.

## Interpretation traps

- Measuring a lock-free structure in a scenario it was not designed for (e.g. using SPSC as MPSC).\n- Oversubscription: more threads than cores.\n- “Optimizing” by removing correctness (e.g. weaker ordering) without proving it is safe.\n\nSee also: [`topics/concurrency/lockfree_notes.md`](../topics/concurrency/lockfree_notes.md).

