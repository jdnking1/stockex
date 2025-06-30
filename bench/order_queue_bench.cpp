#include <models/order.hpp>
#include <chrono>
#include <iostream>
#include <random>
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <ranges>
#include <cstdio>  // for std::printf if you want, or keep std::print with C++20

using namespace stockex::models;

BasicOrder makeOrder(uint32_t i) {
  return BasicOrder{i, static_cast<Quantity>(i % 1000 + 1), static_cast<ClientId>(i * 2), false};
}

template<typename F>
void timeFunctionWithLatency(F func, int iterations, std::vector<double>& latencies) {
  latencies.reserve(iterations);
  for (int i = 0; i < iterations; ++i) {
    auto start = std::chrono::high_resolution_clock::now();
    func(i);
    auto end = std::chrono::high_resolution_clock::now();
    double duration_us = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1000.0;
    latencies.push_back(duration_us);
  }
}

void printMetrics(std::vector<double> &latencies, size_t totalMatches) {
  std::ranges::sort(latencies);
  auto size = static_cast<double>(latencies.size());
  auto sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
  auto minLat = latencies.front();
  auto maxLat = latencies.back();
  auto avg = sum / size;
  auto p99index = static_cast<int>(size * 0.99);
  auto p99 = latencies[p99index];
  auto var = std::accumulate(
      latencies.begin(), latencies.end(), 0.0,
      [avg](double acc, double v) { return acc + (v - avg) * (v - avg); });
  auto stddev = std::sqrt(var / size);
  auto throughput = static_cast<double>(totalMatches) / (sum / 1'000'000.0);

  std::printf("Total time: %.3f us\n", sum);
  std::printf("Total matches: %zu\n", totalMatches);
  std::printf("Average latency: %.3f us\n", avg);
  std::printf("99th percentile latency: %.3f us\n", p99);
  std::printf("Min latency: %.3f us\n", minLat);
  std::printf("Max latency: %.3f us\n", maxLat);
  std::printf("Standard deviation: %.3f us\n", stddev);
  std::printf("Throughput: %.3f matches/sec\n", throughput);
}

void benchmarkPushOnly(int N) {
  OrderQueue queue(N);
  std::vector<double> latencies;
  timeFunctionWithLatency([&](int i){
    queue.push(makeOrder(i));
  }, N, latencies);
  std::cout << "Push only metrics:\n";
  printMetrics(latencies, N);
}

void benchmarkPushPop(int N) {
  OrderQueue queue(N);
  std::vector<double> latencies;
  timeFunctionWithLatency([&](int i){
    auto pos = queue.push(makeOrder(i));
    const auto* front = queue.front();
    if (front) queue.pop();
    (void)pos; (void)front;
  }, N, latencies);
  std::cout << "Push + pop metrics:\n";
  printMetrics(latencies, N);
}

void benchmarkPushRemove(int N) {
  OrderQueue queue(N);
  std::vector<QueuePosition> positions;
  positions.reserve(N);
  std::mt19937 rng(12345);
  std::uniform_int_distribution<int> dist(0, 9);

  std::vector<double> latencies;
  timeFunctionWithLatency([&](int i){
    auto pos = queue.push(makeOrder(i));
    positions.push_back(pos);
    if (dist(rng) == 0 && !positions.empty()) {
      queue.remove(positions.back());
      positions.pop_back();
    }
  }, N, latencies);
  std::cout << "Push + remove (cancel) metrics:\n";
  printMetrics(latencies, N);
}

void benchmarkFullLifecycle(int N) {
  OrderQueue queue(N);
  std::vector<QueuePosition> positions;
  positions.reserve(N);
  std::mt19937 rng(12345);
  std::uniform_int_distribution<int> dist(0, 9);

  std::vector<double> latencies;
  timeFunctionWithLatency([&](int i){
    auto pos = queue.push(makeOrder(i));
    positions.push_back(pos);

    if (positions.size() % 10 == 0) {
      queue.remove(positions.back());
      positions.pop_back();
    }

    auto* front = queue.front();
    if (front) queue.pop();
  }, N, latencies);
  std::cout << "Full lifecycle simulation metrics:\n";
  printMetrics(latencies, N);
}

int main() {
  constexpr int N = 5'000'000;

  //benchmarkPushOnly(N);
  //benchmarkPushPop(N);
  //benchmarkPushRemove(N);
  benchmarkFullLifecycle(N);

  return 0;
}
