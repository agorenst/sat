#include "ema.h"
void ema_restart_t::reset() {
  alpha_incremental = 1;
  counter = 0;
  ema_fast = 0;
  ema_slow = 0;
}
bool ema_restart_t::should_restart() const {
  if (counter < 50) return false;
  if (ema_fast > c * ema_slow) return true;
  return false;
}
void ema_restart_t::step(const size_t lbd) {
  // Update the emas

  if (alpha_incremental > alpha_fast) {
    ema_fast = alpha_incremental * lbd + (1.0f - alpha_incremental) * ema_fast;
  } else {
    ema_fast = alpha_fast * lbd + (1.0f - alpha_fast) * ema_fast;
  }

  if (alpha_incremental > alpha_slow) {
    ema_slow = alpha_incremental * lbd + (1.0f - alpha_incremental) * ema_slow;
  } else {
    ema_slow = alpha_slow * lbd + (1.0f - alpha_slow) * ema_slow;
  }
  alpha_incremental *= 0.5;

  // std::cerr << lbd << "; " << ema_fast << "; " << ema_slow <<
  // std::endl;
  counter++;
}