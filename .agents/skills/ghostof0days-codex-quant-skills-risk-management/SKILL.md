---
name: risk-management
description: "Risk management workflows for limit governance, escalation policy, and live control operations across trading desks. use when tasks involve operational risk controls, limit management, and incident response."
---

# Risk Management
## objective
Operate real-time risk controls with clear escalation and remediation workflows.

## workflow
1. define limit hierarchy and ownership model across desks.
2. monitor live exposures, pnl, and rule-trigger events.
3. escalate breaches with documented action timelines.
4. coordinate remediation and de-risking actions with trading.
5. close incidents only after post-mortem and control updates.

## required diagnostics
- limit breach frequency, duration, and severity trends.
- escalation response-time and closure metrics.
- control false-positive and false-negative analysis.
- cross-desk exposure contagion diagnostics.
- incident recurrence rate after remediation.

## risk controls
- enforce hard stop limits with automated blocking.
- enforce mandatory escalation acknowledgements.
- enforce post-incident control enhancement tracking.

## outputs
- run `python scripts/risk_management_diagnostics.py input.csv --output diagnostics.json` and keep the json artifact.
- write an implementation memo using `references/risk-management-playbook.md` with assumptions, tests, limits, and rollout plan.

## resources
- use `scripts/risk_management_diagnostics.py` for deterministic diagnostics.
- use `references/risk-management-playbook.md` for the domain checklist and delivery structure.
