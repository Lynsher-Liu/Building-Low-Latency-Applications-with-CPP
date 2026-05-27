---
name: feed-handler-normalization
description: "Feed Handler Normalization workflows for quantitative research, implementation, and production controls. use when tasks involve feed and handler workflows in production trading systems."
---

# Feed Handler Normalization
## objective
Execute feed handler normalization work with reproducible research, explicit controls, and deployable outputs.

## workflow
1. define source contracts, schema versions, and freshness objectives.
2. ingest data with replay support and deterministic normalization.
3. validate keys, timestamps, and point-in-time join behavior.
4. monitor quality metrics continuously and quarantine degraded feeds.
5. publish only when lineage, ownership, and quality thresholds are satisfied.

## required diagnostics
- freshness, completeness, null-rate, and duplicate-rate trends.
- schema drift and breaking-change frequency across sources.
- point-in-time join integrity for features and labels.
- backfill and replay consistency versus canonical snapshots.

## risk controls
- enforce hard thresholds for freshness and data-quality metrics.
- enforce quarantine and fallback paths for corrupted feeds.
- enforce full lineage metadata before downstream release.

## outputs
- run `python scripts/feed_handler_normalization_diagnostics.py input.csv --output diagnostics.json` and keep the json artifact.
- write an implementation memo using `references/feed-handler-normalization-playbook.md` with assumptions, tests, limits, and rollout plan.

## resources
- use `scripts/feed_handler_normalization_diagnostics.py` for deterministic diagnostics.
- use `references/feed-handler-normalization-playbook.md` for the domain-specific checklist and delivery structure.
