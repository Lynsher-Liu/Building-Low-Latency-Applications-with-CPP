# Microstructure HFT playbook

## mission
Run microstructure hft workflows for queue dynamics, spread resilience, and impact decomposition with reproducible diagnostics and controlled release criteria.

## required input schema
- `timestamp`
- `instrument`
- `venue`
- `side`
- `order_size`
- `fill_price`
- `benchmark_price`
- `latency_us`
- `spread_bps`

## validation checklist
- benchmark-relative slippage by venue, session, and order urgency.
- fill-rate and queue-position decay under volatility shocks.
- latency tail behavior with packet loss and feed-delay scenarios.
- fee, rebate, and borrow assumptions reflected in net execution cost.

## release controls
- enforce max participation, max order size, and max tolerated slippage.
- enforce kill-switch conditions for stale books, feed gaps, and venue disconnects.
- enforce escalation paths for sustained degradation in fill quality.

## delivery package
- objective, mandate constraints, and benchmark definition
- data lineage, assumptions, and preprocessing decisions
- model or process specification with parameter settings
- diagnostic results, stress tests, and failure analysis
- production rollout, fallback criteria, and ownership
