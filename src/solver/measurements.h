#include <chrono>

struct timer {
  enum class action {
    literal_falsed,
    last_action,
  };
  static const constexpr int to_int(action a) { return static_cast<size_t>(a); }
  static std::chrono::duration<double> cumulative_time[];
  static size_t counter[];
  static void initialize();
  action a;
  std::chrono::time_point<std::chrono::steady_clock> start;
  timer(action a) : a(a), start(std::chrono::steady_clock::now()) {}
  ~timer();
};
