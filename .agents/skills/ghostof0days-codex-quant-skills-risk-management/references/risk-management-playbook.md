# Risk Management playbook

## mission
Operate real-time risk controls with clear escalation and remediation workflows.

## required input schema
- `timestamp`
- `desk`
- `gross_exposure`
- `net_exposure`
- `limit_value`
- `breach_flag`
- `incident_id`

## validation checklist
- limit breach frequency, duration, and severity trends.
- escalation response-time and closure metrics.
- control false-positive and false-negative analysis.
- cross-desk exposure contagion diagnostics.
- incident recurrence rate after remediation.

## release controls
- enforce hard stop limits with automated blocking.
- enforce mandatory escalation acknowledgements.
- enforce post-incident control enhancement tracking.

## delivery package
- objective and constraints
- data lineage and assumptions
- process or model specification
- diagnostics and stress outcomes
- production controls and rollback path
