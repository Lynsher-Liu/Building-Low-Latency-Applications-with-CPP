# Smart Order Routing playbook

## mission
Run smart order routing workflows for instruction design, sizing cadence, and adverse-selection control with reproducible diagnostics and controlled release criteria.

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
- `route_id`
- `route_fill_rate`

## validation checklist
- benchmark-relative slippage by venue, session, and order urgency.
- fill-rate and queue-position decay under volatility shocks.
- latency tail behavior with packet loss and feed-delay scenarios.
- fee, rebate, and borrow assumptions reflected in net execution cost.
- venue routing drift and missed-fill attribution

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
