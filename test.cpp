#include <atomic>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "concurrent_hash_table.h"

int main() {
  std::cout << "hello\n\n";
  ConcurrentHashTable<std::string, std::vector<int>> sequences;
  const std::vector<int> value = {1, 2, 3, 4, 5};
  constexpr auto iterations = ITERATIONS;
  constexpr auto num_workers = 100;

  std::atomic<unsigned long long> insert_ok = 0;
  std::atomic<unsigned long long> insert_no = 0;
  std::atomic<unsigned long long> lookup_ok = 0;
  std::atomic<unsigned long long> lookup_no = 0;

  const auto worker = [&](std::string insert_key, std::string lookup_key) {
    return [&, insert_key = std::move(insert_key),
            lookup_key = std::move(lookup_key)]() {
      std::vector<int> dummy;
      for (int i = 0; i < iterations; ++i) {
        sequences.insert(insert_key, value) ? ++insert_ok : ++insert_no;
        sequences.lookup(dummy, lookup_key) ? ++lookup_ok : ++lookup_no;
      }
    };
  };

  const std::string keys[] = {"how", "now", "brown", "cow",   "grazing",
                              "in",  "the", "green", "green", "grass"};
  std::srand(0);
  const auto rand_key = [&keys]() {
    // It's biased, but so is everybody, right?
    return keys[std::rand() % std::size(keys)];
  };

  std::vector<std::thread> workers;
  for (int i = 0; i < num_workers; ++i) {
    // Pseudo-randomly select separate elements from `keys` for this worker
    // to insert and lookup, respectively.
    workers.push_back(std::thread{worker(rand_key(), rand_key())});
  }

  for (auto& thread : workers) {
    thread.join();
  }

  std::cout << "insert_ok: " << insert_ok << '\n'
            << "insert_no: " << insert_no << '\n'
            << "lookup_ok: " << lookup_ok << '\n'
            << "lookup_no: " << lookup_no << '\n';

  std::cout << "\ngoodbye\n";
}
