#pragma once

#include <print>
#include <string>
#include <thread>
#include <unistd.h>

namespace stockex::benchmarks {
enum class PerfMode { None, Record, Stat };

inline void runPerf(PerfMode mode, const std::string &testName) {
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

inline PerfMode parsePerfMode(int argc, char **argv) {
  using enum PerfMode;
  if (argc < 3)
    return None;
  std::string flag = argv[2];
  if (flag == "--perf=record")
    return Record;
  if (flag == "--perf=stat")
    return Stat;
  if (flag == "--perf=none")
    return None;
  std::print("Unknown perf mode: {}\n", flag);
  exit(1);
}
} // namespace stockex::benchmarks
