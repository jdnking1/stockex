#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <immintrin.h>
#include <print>
#include <vector>

#include "bench_utils.hpp"
#include "engine/order_book.hpp"
#include "simulation_event.hpp"

using namespace stockex::benchmarks;

auto getNsPerCycle() -> double {
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

auto loadEvents(const std::string &filename) -> std::vector<SimulationEvent> {
  std::ifstream file(filename, std::ios::binary | std::ios::ate);
  if (!file) {
    std::println("Cannot open file: {}", filename);
    exit(1);
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  if (size % sizeof(SimulationEvent) != 0) {
    std::println("File corrupt: Size not multiple of Event struct.");
    exit(1);
  }

  std::vector<SimulationEvent> events(size / sizeof(SimulationEvent));

  if (!file.read(reinterpret_cast<char *>(events.data()), size)) {
    std::println("Read error");
    exit(1);
  }
  return events;
}

void processAndSave(const std::vector<uint64_t> &cycles,
                    const std::string &name, double nsPerCycle) {
  if (cycles.empty())
    return;

  std::vector<double> latenciesNs;
  latenciesNs.reserve(cycles.size());
  for (auto c : cycles)
    latenciesNs.push_back(c * nsPerCycle);

  std::println("\n--- {} Latency Statistics ---", name);
  stockex::benchmarks::printMetrics(latenciesNs, latenciesNs.size());

  std::string filename = "replay_latencies_" + name + ".txt";
  stockex::benchmarks::saveLatenciesToFile(latenciesNs, filename);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::println("Usage: {} <dataset_file>\n", argv[0]);
    return 1;
  }

  if (!stockex::utils::pinToCore(4)) {
    return 1;
  }

  auto nsPerCycle = getNsPerCycle();

  std::println("Loading dataset...");
  auto events = loadEvents(argv[1]);
  std::println("Loaded {} events.", events.size());

  auto countAdd = 0;
  auto countCancel = 0;
  auto countMatch = 0;

  for (const auto &e : events) {
    switch (e.type) {
    case EventType::ADD:
      countAdd++;
      break;
    case EventType::CANCEL:
      countCancel++;
      break;
    case EventType::MATCH:
      countMatch++;
      break;
    default:
      break;
    }
  }

  std::vector<uint64_t> latenciesAdd;
  latenciesAdd.reserve(countAdd);
  std::vector<uint64_t> latenciesCancel;
  latenciesCancel.reserve(countCancel);
  std::vector<uint64_t> latenciesMatch;
  latenciesMatch.reserve(countMatch);

  auto book = std::make_unique<stockex::engine::OrderBook>(1);

  std::println("Starting Replay...");

  // WARMUP
  volatile auto dummy = 0;
  for (const auto &e : events) {
    dummy += e.orderId;
  }

  for (const auto &evt : events) {
    uint64_t start;
    uint64_t end;

    if (evt.type == EventType::PREFILL) {
      book->addOrder(evt.clientId, evt.orderId, evt.orderId, evt.side,
                     evt.price, evt.qty);
      continue;
    }

    switch (evt.type) {
    case EventType::PREFILL:
      break;
    case EventType::ADD:
      _mm_lfence();
      start = __rdtsc();
      book->addOrder(evt.clientId, evt.orderId, evt.orderId, evt.side,
                     evt.price, evt.qty);
      _mm_lfence();
      end = __rdtsc();
      _mm_lfence();
      latenciesAdd.push_back(end - start);
      break;

    case EventType::CANCEL:
      _mm_lfence();
      start = __rdtsc();
      book->removeOrder(evt.clientId, evt.orderId);
      _mm_lfence();
      end = __rdtsc();
      _mm_lfence();
      latenciesCancel.push_back(end - start);
      break;

    case EventType::MATCH:
      _mm_lfence();
      start = __rdtsc();
      auto res =
          book->match(evt.clientId, evt.orderId, evt.side, evt.price, evt.qty);
      _mm_lfence();
      end = __rdtsc();
      _mm_lfence();
      if (res.remainingQuantity_ != evt.qty) {
        latenciesMatch.push_back(end - start);
      }
      break;
    }
  }

  std::println("Replay Complete. Processing results...");

  processAndSave(latenciesAdd, "ADD", nsPerCycle);
  processAndSave(latenciesCancel, "CANCEL", nsPerCycle);
  processAndSave(latenciesMatch, "MATCH", nsPerCycle);

  return 0;
}