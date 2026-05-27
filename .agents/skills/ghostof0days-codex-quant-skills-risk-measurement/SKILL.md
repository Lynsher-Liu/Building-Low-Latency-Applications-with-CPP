---
name: risk-measurement
description: "Risk measurement workflows for estimating VaR, expected shortfall, drawdown risk, and exposure distributions across trading books. use when tasks involve quantifying risk metrics rather than managing governance processes."
---

# Risk Measurement
## objective
Measure portfolio and strategy risk with calibrated statistical and scenario-based metrics.

## workflow
1. define horizon, confidence levels, and risk-factor mapping.
2. estimate historical and parametric VaR/ES metrics.
3. run stress scenarios and tail-loss decomposition.
4. backtest risk forecasts against realized pnl outcomes.
5. publish metric sets only after calibration and exception checks.

## required diagnostics
- VaR and ES calibration error by book and horizon.
- exception rate and clustering diagnostics.
- tail dependence and correlation-break analysis.
- stress-loss attribution by factor family.
- drawdown distribution and recovery-time statistics.

## risk controls
- enforce model versioning and reproducible risk runs.
- enforce exception-triggered recalibration rules.
- enforce data completeness checks before measurement runs.

## outputs
- run `python scripts/risk_measurement_diagnostics.py input.csv --output diagnostics.json` and keep the json artifact.
- write an implementation memo using `references/risk-measurement-playbook.md` with assumptions, tests, limits, and rollout plan.

## resources
- use `scripts/risk_measurement_diagnostics.py` for deterministic diagnostics.
- use `references/risk-measurement-playbook.md` for the domain checklist and delivery structure.
