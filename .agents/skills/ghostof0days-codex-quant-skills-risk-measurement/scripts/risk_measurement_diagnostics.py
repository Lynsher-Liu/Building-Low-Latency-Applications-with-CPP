# diagnostics for risk measurement.

import argparse
import json

import pandas as pd


required_columns = ('timestamp', 'book_id', 'pnl', 'var_95', 'var_99', 'expected_shortfall', 'stress_loss')
numeric_columns = ('pnl', 'var_95', 'var_99', 'expected_shortfall', 'stress_loss', 'gross_exposure', 'net_exposure')


def summarize(data_frame):
    working_frame = data_frame.copy()
    summary = {
        "rows": int(len(working_frame)),
        "columns": list(working_frame.columns),
    }

    for column_name in numeric_columns:
        if column_name in working_frame.columns:
            working_frame[column_name] = pd.to_numeric(working_frame[column_name], errors="coerce")

    for column_name in numeric_columns:
        if column_name in working_frame.columns:
            column_series = working_frame[column_name].dropna()
            if not column_series.empty:
                summary[f"{column_name}_mean"] = float(column_series.mean())
                summary[f"{column_name}_p95"] = float(column_series.quantile(0.95))

    if {"pnl", "timestamp"}.issubset(working_frame.columns):
        pnl_series = pd.to_numeric(working_frame["pnl"], errors="coerce").dropna()
        if not pnl_series.empty:
            cumulative_series = pnl_series.cumsum()
            drawdown_series = cumulative_series - cumulative_series.cummax()
            summary["max_drawdown"] = float(drawdown_series.min())

    return summary


def parse_arguments():
    parser = argparse.ArgumentParser(description="diagnostics for risk measurement")
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
