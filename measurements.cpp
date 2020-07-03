#include "measurements.h"
#include <algorithm>
#include <iterator>

std::chrono::duration<double>
    timer::cumulative_time[static_cast<int>(action::last_action)];
size_t timer::counter[static_cast<int>(action::last_action)];

void timer::initialize() {
  std::fill(std::begin(cumulative_time), std::end(cumulative_time),
            std::chrono::duration<double>(std::chrono::seconds(0)));
  std::fill(std::begin(counter), std::end(counter), 0);
}

timer::~timer() {
  auto end = std::chrono::steady_clock::now();
  auto duration = end - start;
  counter[static_cast<int>(a)]++;
  cumulative_time[static_cast<int>(a)] += duration;
}
