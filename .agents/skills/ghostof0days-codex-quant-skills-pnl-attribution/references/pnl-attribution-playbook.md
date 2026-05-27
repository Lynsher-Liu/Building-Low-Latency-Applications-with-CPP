# P&L Attribution playbook

## mission
Run p&l attribution workflows for pnl and attribution workflows in production trading systems with reproducible diagnostics and controlled release criteria.

## required input schema
- `timestamp`
- `portfolio_name`
- `pnl`
- `gross_exposure`
- `net_exposure`
- `var_95`
- `expected_shortfall_97_5`
- `limit_breach_count`

## validation checklist
- limit-breach frequency and concentration by strategy and desk.
- tail-risk evolution across volatility and liquidity regimes.
- scenario-loss decomposition by factor and instrument class.
- control effectiveness and incident-response latency.

## release controls
- enforce hard and soft limits with automated blocking paths.
- enforce intraday breach escalation and documented owner actions.
- enforce independent model and control validation cadences.

## delivery package
- objective, mandate constraints, and benchmark definition
- data lineage, assumptions, and preprocessing decisions
- model or process specification with parameter settings
- diagnostic results, stress tests, and failure analysis
- production rollout, fallback criteria, and ownership
