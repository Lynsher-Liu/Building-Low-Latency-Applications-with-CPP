---
name: pnl-attribution
description: "P&L Attribution workflows for quantitative research, implementation, and production controls. use when tasks involve pnl and attribution workflows in production trading systems."
---

# P&L Attribution
## objective
Execute p&l attribution work with reproducible research, explicit controls, and deployable outputs.

## workflow
1. define risk appetite, limit hierarchy, and escalation rules.
2. aggregate exposures across products, venues, and legal entities.
3. measure pnl, tail risk, and scenario outcomes with daily replay.
4. investigate breaches with root-cause attribution and remediation plans.
5. approve production only with auditable controls and rollback procedures.

## required diagnostics
- limit-breach frequency and concentration by strategy and desk.
- tail-risk evolution across volatility and liquidity regimes.
- scenario-loss decomposition by factor and instrument class.
- control effectiveness and incident-response latency.

## risk controls
- enforce hard and soft limits with automated blocking paths.
- enforce intraday breach escalation and documented owner actions.
- enforce independent model and control validation cadences.

## outputs
- run `python scripts/pnl_attribution_diagnostics.py input.csv --output diagnostics.json` and keep the json artifact.
- write an implementation memo using `references/pnl-attribution-playbook.md` with assumptions, tests, limits, and rollout plan.

## resources
- use `scripts/pnl_attribution_diagnostics.py` for deterministic diagnostics.
- use `references/pnl-attribution-playbook.md` for the domain-specific checklist and delivery structure.
