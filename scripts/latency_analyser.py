import argparse
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path

def main():
    """
    Parses command-line arguments to load latency data from one or more files
    and generates a comparative distribution plot, either saving it or showing it interactively.
    """
    parser = argparse.ArgumentParser(
        description="Compare latency distributions from one or more result files.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "files",
        metavar="FILE",
        nargs="+",
        help="One or more latency data files to compare."
    )
    parser.add_argument(
        "-o", "--output",
        default="latency_comparison.png",
        help="Output filename for the plot (used if --show is not specified)."
    )
    parser.add_argument(
        "-t", "--title",
        default="Latency Distribution Comparison",
        help="Title for the plot."
    )
    parser.add_argument(
        "--clip",
        type=float,
        default=0.99,
        help="Upper percentile to clip the plot for better readability (e.g., 0.99 for 99th percentile)."
    )
    parser.add_argument(
        "-s", "--show",
        action="store_true",
        help="Display the plot interactively instead of saving it to a file."
    )

    args = parser.parse_args()
    all_data = []

    print("Loading files...")
    for filepath_str in args.files:
        filepath = Path(filepath_str)
        if not filepath.is_file():
            print(f"Warning: File not found, skipping: {filepath}")
            continue

        label = filepath.name
        print(f"  - Processing: {label}")
        
        try:
            df_file = pd.read_csv(filepath, header=None, names=['latency_us'])
            df_file['label'] = label
            all_data.append(df_file)
        except pd.errors.EmptyDataError:
            print(f"  - Warning: Skipping empty file {filepath}")
            continue

    if not all_data:
        print("Error: No valid data found in the specified files.")
        return

    df = pd.concat(all_data, ignore_index=True)
    df['latency_ns'] = df['latency_us'] * 1000

    print("\nGenerating plot...")
    plt.style.use('seaborn-v0_8-whitegrid')
    plt.figure(figsize=(12, 7))
    upper_limit = df['latency_ns'].quantile(args.clip)
    sns.kdeplot(data=df, x='latency_ns', hue='label', fill=True, common_norm=False, clip=(0, upper_limit))
    plt.title(args.title, fontsize=16)
    plt.xlabel('Latency (nanoseconds)', fontsize=12)
    plt.ylabel('Density', fontsize=12)
    plt.legend(title='Files')
    
    if args.show:
        print("Displaying plot interactively...")
        plt.show()
    else:
        output_path = Path(args.output)
        plt.savefig(output_path, dpi=150)
        print(f"✅ Plot saved successfully to: {output_path}")

if __name__ == "__main__":
    main()