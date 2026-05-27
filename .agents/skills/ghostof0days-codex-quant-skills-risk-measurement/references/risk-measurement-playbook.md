# Risk Measurement playbook

## mission
Measure portfolio and strategy risk with calibrated statistical and scenario-based metrics.

## required input schema
- `timestamp`
- `book_id`
- `pnl`
- `var_95`
- `var_99`
- `expected_shortfall`
- `stress_loss`

## validation checklist
- VaR and ES calibration error by book and horizon.
- exception rate and clustering diagnostics.
- tail dependence and correlation-break analysis.
- stress-loss attribution by factor family.
- drawdown distribution and recovery-time statistics.

## release controls
- enforce model versioning and reproducible risk runs.
- enforce exception-triggered recalibration rules.
- enforce data completeness checks before measurement runs.

## delivery package
- objective and constraints
- data lineage and assumptions
- process or model specification
- diagnostics and stress outcomes
- production controls and rollback path
