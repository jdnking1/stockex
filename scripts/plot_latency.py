import argparse
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path

def main():
    parser = argparse.ArgumentParser(
        description="Visualize latency distribution from benchmark output files (Nanoseconds)."
    )
    parser.add_argument(
        "files", 
        metavar="FILE", 
        nargs="+", 
        help="One or more latency text files (e.g., replay_latencies_ADD.txt)"
    )
    parser.add_argument(
        "-o", "--output", 
        default=None, 
        help="Output filename to save the plot (e.g., plot.png). If not provided, shows interactive window."
    )
    parser.add_argument(
        "--clip", 
        type=float, 
        default=0.995, 
        help="Percentile to clip the X-axis for better visibility (default: 0.995)"
    )
    parser.add_argument(
        "--title",
        default="Latency Distribution",
        help="Title of the chart"
    )

    args = parser.parse_args()

    sns.set_theme(style="whitegrid")
    plt.figure(figsize=(12, 6))

    all_data = []

    print(f"Loading {len(args.files)} files...")

    for file_path in args.files:
        path = Path(file_path)
        if not path.exists():
            print(f"Warning: File {path} not found. Skipping.")
            continue
        
        try:
            df = pd.read_csv(path, header=None, names=['latency_ns'])
            df['Operation'] = path.stem.replace("replay_latencies_", "")
            all_data.append(df)
            print(f"  -> Loaded {path.name}: {len(df)} samples")
        except Exception as e:
            print(f"Error reading {path}: {e}")

    if not all_data:
        print("No data loaded.")
        return

    combined_df = pd.concat(all_data, ignore_index=True)

    upper_limit = combined_df['latency_ns'].quantile(args.clip)

    print("Generating plot...")
    
    sns.kdeplot(
        data=combined_df, 
        x="latency_ns", 
        hue="Operation", 
        fill=True, 
        common_norm=False, 
        alpha=0.4, 
        linewidth=2,
        clip=(0, upper_limit)
    )

    plt.title(args.title, fontsize=16)
    plt.xlabel("Latency (nanoseconds)", fontsize=12)
    plt.ylabel("Density", fontsize=12)
    plt.xlim(0, upper_limit)
    
    plt.tight_layout()

    if args.output:
        plt.savefig(args.output, dpi=300)
        print(f"Plot saved to: {args.output}")
    else:
        print("Displaying plot...")
        plt.show()

if __name__ == "__main__":
    main()