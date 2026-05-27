# diagnostics for feed handler normalization.

import argparse
import json

import pandas as pd


required_columns = ('timestamp', 'source_name', 'record_count', 'error_count', 'freshness_ms', 'null_rate', 'duplicate_rate', 'schema_version')
numeric_columns = ('record_count', 'error_count', 'freshness_ms', 'null_rate', 'duplicate_rate', 'ingest_latency_ms', 'query_latency_ms', 'join_miss_rate')


def summarize(data_frame):
    working_frame = data_frame.copy()
    summary = {
        "rows": int(len(working_frame)),
        "columns": list(working_frame.columns),
    }

    available_numeric_columns = []
    for column_name in numeric_columns:
        if column_name in working_frame.columns:
            converted_series = pd.to_numeric(working_frame[column_name], errors="coerce")
            working_frame[column_name] = converted_series
            if converted_series.notna().any():
                available_numeric_columns.append(column_name)

    for column_name in available_numeric_columns:
        column_series = working_frame[column_name].dropna()
        summary[f"{column_name}_mean"] = float(column_series.mean()) if not column_series.empty else float("nan")
        summary[f"{column_name}_std"] = float(column_series.std(ddof=1)) if len(column_series) > 1 else float("nan")
        summary[f"{column_name}_p95"] = float(column_series.quantile(0.95)) if not column_series.empty else float("nan")

    if {"record_count", "error_count"}.issubset(working_frame.columns):
        aligned_frame = working_frame[["record_count", "error_count"]].dropna()
        safe_denominator = aligned_frame["record_count"].replace(0, pd.NA)
        error_ratio_series = (aligned_frame["error_count"] / safe_denominator).dropna()
        summary["error_ratio_mean"] = float(error_ratio_series.mean()) if not error_ratio_series.empty else float("nan")

    quality_components = []
    for quality_name in ["null_rate", "duplicate_rate", "join_miss_rate"]:
        if quality_name in working_frame.columns:
            quality_components.append(pd.to_numeric(working_frame[quality_name], errors="coerce"))
    if quality_components:
        quality_frame = pd.concat(quality_components, axis=1)
        quality_series = quality_frame.mean(axis=1, skipna=True).dropna()
        summary["quality_issue_rate_mean"] = float(quality_series.mean()) if not quality_series.empty else float("nan")

    return summary


def parse_arguments():
    parser = argparse.ArgumentParser(description="diagnostics for feed handler normalization")
    parser.add_argument("input_csv", help="input csv file")
    parser.add_argument("--output", help="optional json output file")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    data_frame = pd.read_csv(arguments.input_csv)

    missing_columns = [column_name for column_name in required_columns if column_name not in data_frame.columns]
    if missing_columns:
        raise SystemExit("missing required columns: " + ", ".join(missing_columns))

    data_frame["timestamp"] = pd.to_datetime(data_frame["timestamp"], errors="coerce")
    data_frame = data_frame.dropna(subset=["timestamp"])

    payload = json.dumps(summarize(data_frame), indent=2, sort_keys=True)
    if arguments.output:
        with open(arguments.output, "w", encoding="utf-8") as output_file:
            output_file.write(payload + "\n")
    print(payload)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
