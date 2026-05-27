# diagnostics for pnl attribution.

import argparse
import json

import pandas as pd


required_columns = ('timestamp', 'portfolio_name', 'pnl', 'gross_exposure', 'net_exposure', 'var_95', 'expected_shortfall_97_5', 'limit_breach_count')
numeric_columns = ('pnl', 'gross_exposure', 'net_exposure', 'var_95', 'expected_shortfall_97_5', 'limit_breach_count', 'stress_loss', 'liquidity_horizon_days')


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

    if {"pnl", "var_95"}.issubset(working_frame.columns):
        aligned_frame = working_frame[["pnl", "var_95"]].dropna()
        breach_series = (aligned_frame["pnl"] < -aligned_frame["var_95"]).astype(float)
        summary["var_breach_rate"] = float(breach_series.mean()) if not breach_series.empty else float("nan")

    if "limit_breach_count" in working_frame.columns:
        breach_series = pd.to_numeric(working_frame["limit_breach_count"], errors="coerce").dropna()
        summary["limit_breach_total"] = float(breach_series.sum()) if not breach_series.empty else float("nan")

    return summary


def parse_arguments():
    parser = argparse.ArgumentParser(description="diagnostics for pnl attribution")
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
