# diagnostics for strategy monitoring incident response.

import argparse
import json

import pandas as pd


required_columns = ('timestamp', 'instrument', 'signal_value', 'forward_return', 'position_size', 'pnl', 'turnover', 'gross_exposure')
numeric_columns = ('signal_value', 'forward_return', 'position_size', 'pnl', 'turnover', 'gross_exposure', 'net_exposure', 'drawdown')


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

    signal_column_name = None
    for candidate_name in ["signal_value", "signal", "trend_score", "zscore"]:
        if candidate_name in working_frame.columns:
            signal_column_name = candidate_name
            break

    if signal_column_name and "forward_return" in working_frame.columns:
        aligned_frame = working_frame[[signal_column_name, "forward_return"]].dropna()
        summary["signal_forward_corr"] = float(aligned_frame[signal_column_name].corr(aligned_frame["forward_return"])) if len(aligned_frame) > 1 else float("nan")

    if "pnl" in working_frame.columns:
        pnl_series = working_frame["pnl"].dropna()
        cumulative_series = pnl_series.cumsum()
        drawdown_series = cumulative_series - cumulative_series.cummax()
        summary["max_drawdown"] = float(drawdown_series.min()) if not drawdown_series.empty else float("nan")

    return summary


def parse_arguments():
    parser = argparse.ArgumentParser(description="diagnostics for strategy monitoring incident response")
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
