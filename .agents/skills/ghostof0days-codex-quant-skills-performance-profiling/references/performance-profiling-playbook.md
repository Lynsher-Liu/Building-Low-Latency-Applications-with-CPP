# Performance Profiling playbook

## mission
Run performance profiling workflows for performance and profiling workflows in production trading systems with reproducible diagnostics and controlled release criteria.

## required input schema
- `timestamp`
- `stage_name`
- `latency_us`
- `jitter_us`
- `throughput_messages`
- `drop_rate`
- `cpu_utilization`
- `memory_utilization`

## validation checklist
- stage-level p50, p99, and p999 latency decomposition.
- jitter and throughput stability under sustained burst load.
- packet-loss recovery time and replay correctness.
- resource saturation signals before service-level breach.

## release controls
- enforce hard latency and packet-loss service objectives.
- enforce automatic failover and load-shedding thresholds.
- enforce runbooks for exchange-connectivity incidents.

## delivery package
- objective, mandate constraints, and benchmark definition
- data lineage, assumptions, and preprocessing decisions
- model or process specification with parameter settings
- diagnostic results, stress tests, and failure analysis
- production rollout, fallback criteria, and ownership
