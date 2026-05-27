# Feed Handler Normalization playbook

## mission
Run feed handler normalization workflows for feed and handler workflows in production trading systems with reproducible diagnostics and controlled release criteria.

## required input schema
- `timestamp`
- `source_name`
- `record_count`
- `error_count`
- `freshness_ms`
- `null_rate`
- `duplicate_rate`
- `schema_version`

## validation checklist
- freshness, completeness, null-rate, and duplicate-rate trends.
- schema drift and breaking-change frequency across sources.
- point-in-time join integrity for features and labels.
- backfill and replay consistency versus canonical snapshots.

## release controls
- enforce hard thresholds for freshness and data-quality metrics.
- enforce quarantine and fallback paths for corrupted feeds.
- enforce full lineage metadata before downstream release.

## delivery package
- objective, mandate constraints, and benchmark definition
- data lineage, assumptions, and preprocessing decisions
- model or process specification with parameter settings
- diagnostic results, stress tests, and failure analysis
- production rollout, fallback criteria, and ownership
