#pragma once

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <print>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace stockex::benchmarks {
enum class PerfMode { None, Record, Stat };

inline auto runPerf(PerfMode mode, const std::string &testName) -> void {
  if (pid_t pid = fork(); pid == 0) {
    const auto parentPid = std::to_string(getppid());
    std::print("Running perf on process {} \n", parentPid);

    if (mode == PerfMode::Record) {
      const std::string output = "perf-" + testName + ".record.data";
      execlp("perf", "perf", "record", "-g", "-o", output.c_str(), "-p",
             parentPid.c_str(), (char *)nullptr);
    } else if (mode == PerfMode::Stat) {
      const std::string output = "perf-" + testName + ".stat.txt";
      execlp("perf", "perf", "stat", "-I", "1000", "-p", parentPid.c_str(),
             "-o", output.c_str(), (char *)nullptr);
    }

    std::print("failed to launch perf");
  } else if (pid < 0) {
    std::print("fork failed");
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

inline auto parsePerfMode(const std::string &flag) -> PerfMode {
  using enum PerfMode;
  if (flag == "--perf=record")
    return Record;
  if (flag == "--perf=stat")
    return Stat;
  if (flag == "--perf=none")
    return None;
  std::print("Unknown perf mode: {}\n", flag);
  exit(1);
}

inline auto saveLatenciesToFile(const std::vector<double> &latencies,
                                const std::string &filename) -> void {
  std::ofstream outputFile(filename);
  if (!outputFile.is_open()) {
    std::print(stderr, "Error: Could not open the file {}\n", filename);
    return;
  }
  for (const double &latency : latencies) {
    outputFile << latency << "\n";
  }
  std::print("Successfully saved {} latency values to {}\n", latencies.size(),
             filename);
}

inline auto printMetrics(std::vector<double> &latencies, size_t totalOps)
    -> void {
  if (latencies.empty()) {
    std::print("No matches occurred, cannot compute metrics.\n");
    return;
  }
  std::ranges::sort(latencies);
  auto size = static_cast<double>(latencies.size());
  auto sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
  auto minLat = latencies.front();
  auto maxLat = latencies.back();
  auto avg = sum / size;
  auto p99index = static_cast<std::size_t>(size * 0.99);
  auto p999index = static_cast<std::size_t>(size * 0.999);
  auto p99 = latencies[p99index];
  auto p999 = latencies[p999index];
  auto var = std::accumulate(
      latencies.begin(), latencies.end(), 0.0,
      [avg](double acc, double v) { return acc + (v - avg) * (v - avg); });
  auto stddev = std::sqrt(var / size);
  auto throughput = static_cast<double>(totalOps) / (sum / 1'000'000.0);

  std::print("Total time: {} us\n", sum);
  std::print("Total ops: {}\n", totalOps);
  std::print("Average latency: {} us\n", avg);
  std::print("99th percentile latency: {} us\n", p99);
  std::print("99.9th percentile latency: {} us\n", p999);
  std::print("Min latency: {} us\n", minLat);
  std::print("Max latency: {} us\n", maxLat);
  std::print("Standard deviation: {} us\n", stddev);
  std::print("Throughput: {} ops/sec\n", throughput);
}

} // namespace stockex::benchmarks
