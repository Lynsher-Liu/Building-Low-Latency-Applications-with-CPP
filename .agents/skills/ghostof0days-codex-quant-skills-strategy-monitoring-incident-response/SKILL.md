---
name: strategy-monitoring-incident-response
description: "Strategy Monitoring Incident Response workflows for quantitative research, implementation, and production controls. use when tasks involve strategy and monitoring workflows in production trading systems."
---

# Strategy Monitoring Incident Response
## objective
Execute strategy monitoring incident response work with reproducible research, explicit controls, and deployable outputs.

## workflow
1. define hypothesis, trade horizon, and capital-allocation constraints.
2. build leak-safe features and align targets to executable decision times.
3. estimate signal edge, turnover impact, and capacity limits.
4. stress performance across volatility, liquidity, and crowding regimes.
5. promote only when net performance remains robust after full trading costs.

## required diagnostics
- signal monotonicity, decay profile, and hit-rate stability.
- capacity stress from participation growth and liquidity depletion.
- regime dependency and edge persistence after parameter shifts.
- cost-adjusted performance versus naive and benchmark alternatives.

## risk controls
- enforce gross and net exposure ceilings by strategy and instrument.
- enforce concentration and turnover caps to prevent capacity overload.
- enforce deactivation triggers for edge decay and drawdown breaches.

## outputs
- run `python scripts/strategy_monitoring_incident_response_diagnostics.py input.csv --output diagnostics.json` and keep the json artifact.
- write an implementation memo using `references/strategy-monitoring-incident-response-playbook.md` with assumptions, tests, limits, and rollout plan.

## resources
- use `scripts/strategy_monitoring_incident_response_diagnostics.py` for deterministic diagnostics.
- use `references/strategy-monitoring-incident-response-playbook.md` for the domain-specific checklist and delivery structure.
