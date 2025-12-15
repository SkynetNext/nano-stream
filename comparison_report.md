# Performance Comparison: Nano-Stream (C++) vs Disruptor (Java)

Generated: 2025-12-15 16:41:14 UTC

## C++ Benchmarks (Nano-Stream)

### Key Metrics

| Benchmark | Throughput (ops/sec) | Latency (ns) |
|-----------|---------------------|--------------|
| BM_SequenceGet | 12666870255928429000 | 0.31136662611303573 |
| BM_SequenceSet | 14133118023770749000 | 0.31828592122659027 |
| BM_SequenceIncrementAndGet | 247569835210968260 | 2.3756070665831412 |
| BM_SequenceCompareAndSet | 178146169634195500 | 2.7969031274886222 |
| BM_AtomicLoad | 14469657648669213000 | 0.31111948397853312 |
| BM_AtomicStore | 14372436417092150000 | 0.31199583416958593 |
| BM_AtomicFetchAdd | 245280084854220350 | 2.3894959566258298 |
| BM_SequenceConcurrentIncrements/threads:1 | 300492892833189300 | 2.1675013570506056 |
| BM_SequenceConcurrentIncrements/threads:2 | 2404209906687969.5 | 24.133131570000753 |
| BM_SequenceConcurrentIncrements/threads:4 | 386447654966703.7 | 60.186842127438965 |
| BM_AtomicConcurrentIncrements/threads:1 | 310411707150296200 | 2.1225254003741312 |
| BM_AtomicConcurrentIncrements/threads:2 | 3017248463198327 | 21.592596299126065 |
| BM_AtomicConcurrentIncrements/threads:4 | 460258581397314.6 | 55.305893314841121 |
| BM_RingBufferSingleProducer/1024 | 302857855095486340 | 2.1509224015161048 |
| BM_RingBufferSingleProducer/4096 | 307115036350508100 | 2.1327892205591659 |
| BM_RingBufferSingleProducer/16384 | 298638624267010560 | 2.1662004189438053 |
| BM_RingBufferBatchProducer/1 | 198186013980389700 | 2.6544643763411826 |
| BM_RingBufferBatchProducer/8 | 16456108513344126 | 9.2237142746669178 |
| BM_RingBufferBatchProducer/64 | 294155983641533.8 | 68.981829806078849 |
| BM_RingBufferTryNext | 59512795110434780 | 4.8492737984238765 |

## Summary

### Performance Highlights

- **C++ (Nano-Stream)**: Header-only, zero-allocation, sub-nanosecond latency
- **Java (Disruptor)**: Mature library, JVM-optimized, battle-tested

> Note: Direct comparison requires matching test scenarios. Results may vary based on hardware and JVM tuning.
