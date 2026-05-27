# diagnostics for reinforcement learning trading.

import argparse
import json

import pandas as pd


required_columns = ('timestamp', 'target_value', 'prediction_value', 'residual_value', 'parameter_value', 'objective_value', 'calibration_error', 'simulation_count', 'episode_reward', 'policy_loss', 'value_loss', 'exploration_rate')
numeric_columns = ('target_value', 'prediction_value', 'residual_value', 'parameter_value', 'objective_value', 'calibration_error', 'simulation_count', 'drift_score', 'episode_reward', 'policy_loss', 'value_loss', 'exploration_rate')


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

    if {"target_value", "prediction_value"}.issubset(working_frame.columns):
        aligned_frame = working_frame[["target_value", "prediction_value"]].dropna()
        residual_series = aligned_frame["prediction_value"] - aligned_frame["target_value"]
        summary["mae"] = float(residual_series.abs().mean()) if not residual_series.empty else float("nan")
        summary["rmse"] = float((residual_series.pow(2).mean()) ** 0.5) if not residual_series.empty else float("nan")
        summary["prediction_target_corr"] = float(aligned_frame["prediction_value"].corr(aligned_frame["target_value"])) if len(aligned_frame) > 1 else float("nan")

    return summary


def parse_arguments():
    parser = argparse.ArgumentParser(description="diagnostics for reinforcement learning trading")
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
