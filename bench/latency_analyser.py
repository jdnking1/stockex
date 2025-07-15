import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import numpy as np
import argparse
import os
import json

def plot_latency_analysis(filenames, output_prefix=None, use_log_scale=False, unit='ns'):
    if unit == 'ns':
        scaling_factor = 1000
        unit_label = 'ns'
    else: 
        scaling_factor = 1
        unit_label = 'µs'

    plt.style.use('seaborn-v0_8-darkgrid')
    _, (ax1, ax2) = plt.subplots(2, 1, figsize=(15, 12), sharex=True)
    
    all_stats = {}

    for filename in filenames:
        try:
            with open(filename, 'r') as f:
                latencies = [float(line.strip()) * scaling_factor for line in f if line.strip()]
            if not latencies:
                print(f"Warning: '{filename}' is empty. Skipping.")
                continue

            percentiles_to_calc = [50, 90, 95, 99, 99.9]
            percentile_values = np.percentile(latencies, percentiles_to_calc)
            stats = {
                'mean': np.mean(latencies),
                'max': np.max(latencies),
                'count': len(latencies)
            }
            for p, v in zip(percentiles_to_calc, percentile_values):
                stats[f'p{p}'] = v
            
            clean_name = os.path.basename(filename).replace('.txt', '')
            all_stats[clean_name] = stats
            
            label = f'{clean_name} (p99={stats["p99"]:.2f} {unit_label})'
            ax1.hist(latencies, bins=150, histtype='step', linewidth=2,
                     density=True, label=label)
            
            sorted_latencies = np.sort(latencies)
            yvals = np.arange(1, len(sorted_latencies) + 1) / len(sorted_latencies)
            ax2.plot(sorted_latencies, yvals, linewidth=2, label=clean_name)

        except FileNotFoundError:
            print(f"Error: The file '{filename}' was not found.")
        except Exception as e:
            print(f"An error occurred with file {filename}: {e}")

    if not all_stats:
        print("No data was plotted.")
        return

    plot_limit = max(s['p99.9'] for s in all_stats.values()) * 1.2
    ax1.set_xlim(0, plot_limit)

    ax1.set_title('Latency Distribution Analysis', fontsize=18, fontweight='bold')
    ax1.set_ylabel('Probability Density', fontsize=12)
    ax1.legend()

    ax2.set_title('Cumulative Distribution Function (CDF)', fontsize=16)
    xlabel = f'Latency ({unit_label})'
    if use_log_scale:
        ax1.set_xscale('log')
        ax2.set_xscale('log')
        ax1.set_xlim(left=min(s['p50'] for s in all_stats.values()) * 0.1)
        xlabel += ' [Log Scale]'
    
    ax2.set_xlabel(xlabel, fontsize=12)
    ax2.set_ylabel('Proportion of Requests', fontsize=12)
    ax2.yaxis.set_major_formatter(mticker.PercentFormatter(xmax=1.0))

    print(f"--- Performance Analysis Summary (Unit: {unit_label}) ---")
    formatted_stats = {k: {sk: f"{sv:.2f}" for sk, sv in v.items()} for k, v in all_stats.items()}
    print(json.dumps(formatted_stats, indent=4))
    
    if output_prefix:
        plot_filename = f"{output_prefix}_analysis.png"
        stats_filename = f"{output_prefix}_stats.json"
        
        with open(stats_filename, 'w') as f:
            json.dump(all_stats, f, indent=4)
        print(f"\nSaved statistics to '{stats_filename}'")
        
        plt.savefig(plot_filename, dpi=150)
        print(f"Saved plot to '{plot_filename}'")
    else:
        plt.tight_layout()
        plt.show()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Analyze and plot latency data from text files.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument(
        'filenames',
        metavar='FILE',
        nargs='+',
        help='One or more text files containing latency data (in microseconds).'
    )
    parser.add_argument(
        '--unit',
        type=str,
        choices=['ns', 'us'],
        default='ns',
        help="The unit for the latency axis:\n'ns' for nanoseconds (default)\n'us' for microseconds"
    )
    parser.add_argument(
        '--output',
        '-o',
        type=str,
        help='Optional prefix for saving the plot and stats JSON file.'
    )
    parser.add_argument(
        '--log',
        action='store_true',
        help='Use a logarithmic scale for the x-axis (latency).'
    )
    
    args = parser.parse_args()
    
    plot_latency_analysis(args.filenames, args.output, args.log, args.unit)