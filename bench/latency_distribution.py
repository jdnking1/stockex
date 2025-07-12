import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import numpy as np
import sys
import os

def plot_latency_distribution(filename):
    try:
        with open(filename, 'r') as f:
            latencies = [float(line.strip()) * 1000 for line in f if line.strip()]

        if not latencies:
            print(f"Error: The file '{filename}' is empty or contains no valid data.")
            return

        mean_latency = np.mean(latencies)
        p99_latency = np.percentile(latencies, 99)
        p999_latency = np.percentile(latencies, 99.9)
        max_latency = np.max(latencies)
        
        plot_limit = p999_latency * 1.1
        plot_latencies = [l for l in latencies if l <= plot_limit]
        
        plt.style.use('seaborn-v0_8-deep')
        _, ax = plt.subplots(figsize=(12, 7))

        ax.hist(plot_latencies, bins=100, histtype='stepfilled', color='darkcyan', alpha=0.8, label='Latency Distribution')

        base_name = os.path.basename(filename)
        title_name = base_name.replace('.txt', '').replace('latencies_', '')
        plot_title = f"Latency Distribution for '{title_name}' Test (View limited to {plot_limit:.0f} ns)"
        
        ax.set_title(plot_title, fontsize=16, fontweight='bold')
        ax.set_xlabel('Latency (ns)', fontsize=12)
        ax.set_ylabel('Frequency', fontsize=12)
        
        formatter = mticker.ScalarFormatter(useMathText=True)
        formatter.set_scientific(True)
        formatter.set_powerlimits((0, 0))
        ax.yaxis.set_major_formatter(formatter)

        ax.axvline(mean_latency, color='red', linestyle='dashed', linewidth=2, label=f'Mean: {mean_latency:.2f} ns')
        ax.axvline(p99_latency, color='orange', linestyle='dashed', linewidth=2, label=f'99th Percentile: {p99_latency:.2f} ns')
        ax.axvline(p999_latency, color='purple', linestyle='dashed', linewidth=2, label=f'99.9th Percentile: {p999_latency:.2f} ns')
        
        ax.legend()
        ax.grid(True, which='both', linestyle='--', linewidth=0.5)

        print(f"Successfully plotted data from '{filename}'.")
        print("Statistics from the complete dataset:")
        print(f"  - Mean Latency: {mean_latency:.2f} ns")
        print(f"  - 99th Percentile: {p99_latency:.2f} ns")
        print(f"  - 99.9th Percentile: {p999_latency:.2f} ns")
        print(f"  - Max Latency: {max_latency:.2f} ns")
        
        plt.tight_layout()
        plt.show()

    except FileNotFoundError:
        print(f"Error: The file '{filename}' was not found.")
    except ValueError:
        print(f"Error: The file '{filename}' contains non-numeric data.")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        file_to_plot = sys.argv[1]
        plot_latency_distribution(file_to_plot)
    else:
        print("Usage: python your_script_name.py <filename>")
        print("Example: python plot_latencies.py latencies_flat.txt")
