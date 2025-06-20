#include <algorithm>
#include <chrono>
#include <ctime>
#include <memory>
#include <stdexcept>
#include <string>
#include <unistd.h>

#include "engine/order_book.hpp"
#include "models/basic_types.hpp"

void runPerf() {
  pid_t pid = fork();
  if (pid == 0) {
    const auto parentPid = std::to_string(getppid());
    std::print("Running perf on process {}\n", parentPid);
    /*execlp("perf", "perf", "stat", "-I", "1000", "-M", "TopdownL1", "-p",
           parentPid.c_str(), (char *)NULL);*/
    /*execlp("perf", "perf", "stat", "-e", "cache-misses", "-p",
           parentPid.c_str(), (char *)NULL);*/
    execlp("perf", "perf", "record", "-g","-p",
           parentPid.c_str(), (char *)NULL);
    /*execlp("perf", "perf", "record", "-e", "cache-misses", "-p",
           parentPid.c_str(), (char *)NULL);*/
    throw std::runtime_error("execlp failed");
  } else if (pid < 0) {
    throw std::runtime_error("fork failed");
  }
}

int main() {
  using enum stockex::models::Side;
  auto book{std::make_unique<stockex::engine::OrderBook>(1)};
  const int numOrders = 7500000;
  for (stockex::models::OrderId i = 0; i < numOrders; ++i) {
    book->addOrder(1, i, i, BUY, 100 + (i % 100), 50);
  }
  /*auto start = std::chrono::high_resolution_clock::now();
  for (stockex::models::OrderId i = numOrders; i < numOrders + numOrders - 1; ++i) {
    book->addOrder(1, i, i, BUY, 100 + (i % 100), 50);
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::micro> duration = end - start;
  std::cout << "addOrder: " << duration.count() / numOrders << " us percall\n";
  std::cout << "addOrder: " << duration.count() << " us total\n";*/
  runPerf();
  /*auto start = std::chrono::high_resolution_clock::now();
  for (stockex::models::OrderId i = numOrders - 1; i > 0; --i) {
    book->removeOrder(1, i);
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::micro> duration = end - start;
  std::cout << "removeOrder: " << duration.count() / numOrders << " us percall\n";
  std::cout << "removeOrder: " << duration.count() << " us total\n";*/
  stockex::engine::MatchResultSet m{};
  auto start = std::chrono::high_resolution_clock::now();
  for (stockex::models::OrderId i = 0; i < numOrders; i++) {
    m = book->match(2, 1, SELL, 100 + (i % 100), 1000);
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::micro> duration = end - start;
  std::cout << m.matches_.size() << std::endl;
  std::cout << "matchOrder: " << duration.count() / numOrders << " us percall\n";
  std::cout << "matchOrder: " << duration.count() << " us total\n";
} 

