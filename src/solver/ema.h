#include <cstddef>
struct ema_restart_t {
  // For restarts...
  const float alpha_fast = 1.0 / 32.0;
  const float alpha_slow = 1.0 / 4096.0;
  const float c = 1.25;

  float alpha_incremental = 1;
  int counter = 0;
  float ema_fast = 0;
  float ema_slow = 0;

  void reset();
  bool should_restart() const;
  void step(const size_t lbd);
};