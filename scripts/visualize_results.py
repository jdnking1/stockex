import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import os
from pathlib import Path
import re

def parse_filename(filename):
    """Parses benchmark metadata from the latency filename."""
    market_sim_match = re.match(r"latencies_(add|cancel|match)_(bitmap_chunked_queueSIMD)_(.*)_(\d+)\.txt", filename)
    if market_sim_match:
        metric, implementation, scenario, _ = market_sim_match.groups()
        return {"metric": metric, "implementation": implementation, "scenario": scenario}

    other_test_match = re.match(r"latencies_(.*)_(bitmap_chunked_queueSIMD)\.txt", filename)
    if other_test_match:
        test_name, implementation = other_test_match.groups()
        metric_name = test_name.replace('_test', '')
        return {"metric": metric_name, "implementation": implementation, "scenario": metric_name}
    return None

def load_data_for_metric(results_dir, metric_to_load):
    """Loads all data for a specific metric."""
    all_data = []
    for filename in os.listdir(results_dir):
        if not filename.endswith(".txt"):
            continue
        
        metadata = parse_filename(filename)
        if metadata and metadata['metric'] == metric_to_load:
            filepath = results_dir / filename
            try:
                df_file = pd.read_csv(filepath, header=None, names=['latency_us'])
                for key, value in metadata.items():
                    df_file[key] = value
                all_data.append(df_file)
            except pd.errors.EmptyDataError:
                print(f"  - Warning: Skipping empty file {filename}")
                continue

    if not all_data:
        return pd.DataFrame()
    
    return pd.concat(all_data, ignore_index=True)


def main():
    """Main function to load data and generate distribution plots."""
    # Determine project root assuming this script is in a 'scripts' directory
    script_dir = Path(__file__).parent.resolve()
    project_root = script_dir.parent

    # A simple check to validate the project root
    if not (project_root / 'CMakeLists.txt').is_file():
        print(f"Error: Could not determine project root from script location '{script_dir}'.")
        print("Please ensure the script is in a 'scripts' directory at the project root.")
        return

    results_dir = project_root / "benchmark_results"
    plots_dir = project_root / "benchmark_plots"

    if not results_dir.exists():
        print(f"Error: Directory '{results_dir}' not found. Please run the benchmarks first.")
        return

    plots_dir.mkdir(exist_ok=True)
    print(f"Plots will be saved in '{plots_dir}'")
    print("-" * 20)

    # --- Generate Plots Metric-by-Metric ---
    print("Generating latency distribution plots...")
    all_metrics = ['add', 'cancel', 'match', 'fragmentation', 'sweep']
    for metric in all_metrics:
        df_metric = load_data_for_metric(results_dir, metric)
        if df_metric.empty:
            continue

        df_metric['latency_ns'] = df_metric['latency_us'] * 1000

        plt.figure(figsize=(12, 7))
        upper_limit = df_metric['latency_ns'].quantile(0.99)
        sns.kdeplot(data=df_metric, x='latency_ns', hue='implementation', fill=True,
                    common_norm=False, clip=(0, upper_limit))

        plt.title(f'Latency Distribution for "{metric.capitalize()}" Operations')
        plt.xlabel('Latency (nanoseconds)')
        plt.ylabel('Density')
        plt.grid(True, which='both', linestyle='--', alpha=0.6)
        plot_filename = plots_dir / f"distribution_{metric}.png"
        plt.savefig(plot_filename, dpi=150)
        plt.close()
        print(f"  -> Saved plot: {plot_filename}")

    print("\n✅ Visualization complete.")

if __name__ == "__main__":
    main()