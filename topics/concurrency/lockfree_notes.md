# Lock-free notes (SPSC focus)

This repo has several “HFT-shaped” building blocks where performance comes from **removing contention** and **controlling memory traffic** rather than clever math. The most important example is **SPSC** (single-producer / single-consumer) queues/rings.

## What SPSC *buys* you

SPSC is a special case where you can be both fast and simple because:

- **Only the producer writes the write index**.
- **Only the consumer writes the read index**.
- The producer never touches consumer-owned data (and vice versa) except by reading the other index.

Those ownership rules are the core “correctness contract”. If you violate them (e.g. 2 producers), the code becomes incorrect even if it “seems to work”.

## The invariant (how to reason about correctness)

For a ring with capacity \(N\):

- **Empty** when `read_idx == write_idx`
- **Full** when `next(write_idx) == read_idx`
- Producer must publish data *before* it publishes the updated `write_idx`
- Consumer must observe the updated `write_idx` *before* it consumes the corresponding data

That “publish order” is what acquire/release enforces.

## Where acquire/release matters (and why)

In [`lockfree_spsc_ring_buffer.cpp`](../concurrency/lockfree_spsc_ring_buffer.cpp):

- Producer path:\n  - reads `read_idx_` with **acquire** to avoid overwriting unread slots\n  - writes the new `write_idx_` with **release** so the consumer sees the slot contents before seeing the index move
- Consumer path:\n  - reads `write_idx_` with **acquire** so it won’t read a slot before the producer has finished writing it\n  - writes the new `read_idx_` with **release** so the producer sees space become available after consumption

The same pattern appears in the domain-style queue in [`market_data_message_queue.cpp`](../domain_sketches/market_data_message_queue.cpp): a fixed-size message is copied into a slot, then the producer advances an index using release semantics.

## False sharing and padding

Even when the algorithm is lock-free, it can still be slow if threads fight over cache lines.

Practical tricks used across the repo:

- `alignas(64)` and `CACHE_LINE = 64` to keep frequently-written atomics on separate cache lines
- fixed-size slots/messages so you don’t allocate per message

See also: `false_sharing.cpp` in [`topics/microarchitecture/`](../microarchitecture/).

## Common failure modes (things to try as exercises)

- Change the code to MPSC (2 producers) without adding the required atomic protocol → watch it break.\n- Remove the acquire/release ordering and replace with relaxed → look for intermittent corruption.\n- Remove cache-line padding from indices and compare throughput under contention.\n- Add backpressure policy: drop newest, drop oldest, or block—then measure tail latency.\n+
## Related code\n+\n+- SPSC ring demo + benchmark: [`lockfree_spsc_ring_buffer.cpp`](../concurrency/lockfree_spsc_ring_buffer.cpp)\n+- Market-data-shaped SPSC queue: [`market_data_message_queue.cpp`](../domain_sketches/market_data_message_queue.cpp)\n+- Teaching/visual ring (slower, more print-heavy): [`spsc_ring_buffer.cpp`](../language/spsc_ring_buffer.cpp)\n+
