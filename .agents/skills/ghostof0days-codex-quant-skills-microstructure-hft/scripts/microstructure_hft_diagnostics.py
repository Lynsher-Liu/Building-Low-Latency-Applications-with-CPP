# diagnostics for microstructure hft.

import argparse
import json

import pandas as pd


required_columns = ('timestamp', 'instrument', 'venue', 'side', 'order_size', 'fill_price', 'benchmark_price', 'latency_us', 'spread_bps')
numeric_columns = ('order_size', 'fill_price', 'benchmark_price', 'latency_us', 'spread_bps', 'slippage_bps', 'fill_rate', 'queue_ahead')


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

    if {"fill_price", "benchmark_price"}.issubset(working_frame.columns):
        aligned_frame = working_frame[["fill_price", "benchmark_price"]].dropna()
        slippage_series = aligned_frame["fill_price"] - aligned_frame["benchmark_price"]
        summary["slippage_mean"] = float(slippage_series.mean()) if not slippage_series.empty else float("nan")

    latency_column_names = [column_name for column_name in available_numeric_columns if column_name.endswith("_us") or column_name.endswith("_ms")]
    if latency_column_names:
        latency_frame = working_frame[latency_column_names].apply(pd.to_numeric, errors="coerce")
        total_latency_series = latency_frame.sum(axis=1, min_count=1).dropna()
        summary["total_latency_mean"] = float(total_latency_series.mean()) if not total_latency_series.empty else float("nan")
        summary["total_latency_p99"] = float(total_latency_series.quantile(0.99)) if not total_latency_series.empty else float("nan")

    return summary


def parse_arguments():
    parser = argparse.ArgumentParser(description="diagnostics for microstructure hft")
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
