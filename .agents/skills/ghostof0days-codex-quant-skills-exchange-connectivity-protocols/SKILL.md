---
name: exchange-connectivity-protocols
description: "Exchange Connectivity Protocols workflows for quantitative research, implementation, and production controls. use when tasks involve exchange and connectivity workflows in production trading systems."
---

# Exchange Connectivity Protocols
## objective
Execute exchange connectivity protocols work with reproducible research, explicit controls, and deployable outputs.

## workflow
1. define execution benchmark, urgency, and participation limits.
2. profile venue liquidity, queue dynamics, and spread behavior before routing.
3. configure order instructions and routing logic with deterministic safeguards.
4. attribute slippage into spread, impact, timing, and opportunity components.
5. deploy only after stable execution quality through stressed market windows.

## required diagnostics
- benchmark-relative slippage by venue, session, and order urgency.
- fill-rate and queue-position decay under volatility shocks.
- latency tail behavior with packet loss and feed-delay scenarios.
- fee, rebate, and borrow assumptions reflected in net execution cost.

## risk controls
- enforce max participation, max order size, and max tolerated slippage.
- enforce kill-switch conditions for stale books, feed gaps, and venue disconnects.
- enforce escalation paths for sustained degradation in fill quality.

## outputs
- run `python scripts/exchange_connectivity_protocols_diagnostics.py input.csv --output diagnostics.json` and keep the json artifact.
- write an implementation memo using `references/exchange-connectivity-protocols-playbook.md` with assumptions, tests, limits, and rollout plan.

## resources
- use `scripts/exchange_connectivity_protocols_diagnostics.py` for deterministic diagnostics.
- use `references/exchange-connectivity-protocols-playbook.md` for the domain-specific checklist and delivery structure.
