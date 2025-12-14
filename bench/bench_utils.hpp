#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <immintrin.h>
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
  auto p50index = static_cast<std::size_t>(size / 2);
  auto p99 = latencies[p99index];
  auto p999 = latencies[p999index];
  auto p50 = latencies[p50index];
  auto var = std::accumulate(
      latencies.begin(), latencies.end(), 0.0,
      [avg](double acc, double v) { return acc + (v - avg) * (v - avg); });
  auto stddev = std::sqrt(var / size);

  auto throughput = static_cast<double>(totalOps) / (sum / 1'000'000'000.0);

  std::print("Total time: {} ns\n", sum);
  std::print("Total ops: {}\n", totalOps);
  std::print("Average latency: {} ns\n", avg);
  std::print("Median latency: {} ns\n", p50);
  std::print("99th percentile latency: {} ns\n", p99);
  std::print("99.9th percentile latency: {} ns\n", p999);
  std::print("Min latency: {} ns\n", minLat);
  std::print("Max latency: {} ns\n", maxLat);
  std::print("Standard deviation: {} ns\n", stddev);
  std::print("Throughput: {} ops/sec\n", throughput);
}

inline auto getNsPerCycle() -> double {
  std::println("Calibrating RDTSC...");
  auto startTime = std::chrono::steady_clock::now();
  auto startCycles = __rdtsc();

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  uint64_t endCycles = __rdtsc();
  auto endTime = std::chrono::steady_clock::now();

  auto durationNs =
      std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime)
          .count();
  auto cycles = endCycles - startCycles;

  auto nsPerCycle = static_cast<double>(durationNs) / cycles;
  std::println("Detected TSC Frequency: {:.2f} GHz (1 cycle = {:.5f} ns)",
               1.0 / nsPerCycle, nsPerCycle);

  return nsPerCycle;
}

inline auto measureOverhead() -> uint64_t {
  uint64_t start;
  uint64_t end;
  uint64_t min_diff = UINT64_MAX;

  for (int i = 0; i < 10000; i++) {
    _mm_lfence();
    start = __rdtsc();
    _mm_lfence();

    _mm_lfence();
    end = __rdtsc();
    _mm_lfence();

    uint64_t diff = end - start;
    if (diff < min_diff)
      min_diff = diff;
  }
  std::println("Detected Measurement Overhead: {} cycles", min_diff);
  return min_diff;
}

#define BENCH_OP(VECTOR, CODE)                                       \
  do {                                                                         \
    uint64_t _start, _end;                                                     \
    _mm_lfence();                                                              \
    _start = __rdtsc();                                                        \
    _mm_lfence();                                                              \
    CODE;                                                                      \
    _mm_lfence();                                                              \
    _end = __rdtsc();                                                          \
    _mm_lfence();                                                              \
    uint64_t _raw = _end - _start;                                             \
    (VECTOR).push_back(_raw);                                                  \
  } while (0)

} // namespace stockex::benchmarks
