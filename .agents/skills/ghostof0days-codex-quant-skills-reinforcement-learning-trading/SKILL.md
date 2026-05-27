---
name: reinforcement-learning-trading
description: "Reinforcement Learning Trading workflows for quantitative research, implementation, and production controls. use when tasks involve policy learning stability, reward shaping, and deployment safeguards."
---

# Reinforcement Learning Trading
## objective
Execute reinforcement learning trading work with reproducible research, explicit controls, and deployable outputs.

## workflow
1. define assumptions, governing equations, and boundary conditions.
2. estimate parameters with reproducible calibration settings.
3. validate residual structure, numerical stability, and convergence behavior.
4. stress model behavior across regime changes and parameter perturbations.
5. release only when out-of-sample accuracy and stability remain within limits.

## required diagnostics
- residual diagnostics and autocorrelation by horizon.
- parameter stability across rolling and expanding windows.
- numerical convergence behavior and solver tolerance sensitivity.
- forecast calibration and distributional fit checks.
- policy collapse risk under sparse-reward regimes
- out-of-distribution action monitoring in production

## risk controls
- enforce parameter-bound and convergence-failure safeguards.
- enforce rollback to baseline models on instability.
- enforce monitoring for drift and structural-break detection.

## outputs
- run `python scripts/reinforcement_learning_trading_diagnostics.py input.csv --output diagnostics.json` and keep the json artifact.
- write an implementation memo using `references/reinforcement-learning-trading-playbook.md` with assumptions, tests, limits, and rollout plan.

## resources
- use `scripts/reinforcement_learning_trading_diagnostics.py` for deterministic diagnostics.
- use `references/reinforcement-learning-trading-playbook.md` for the domain-specific checklist and delivery structure.
