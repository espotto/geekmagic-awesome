// PomodoroCore.h — the Pomodoro app's clock, without Arduino (tests/apps):
// focus sessions with a short break after each and a long break after every
// `rounds` of them. Time is millis() passed in, so the device and the tests
// drive it the same way.
#pragma once
#include <stdint.h>

namespace pomo {

enum class Phase : uint8_t { Focus, Short, Long };
enum class Run : uint8_t { Idle, Running, Paused };

// The durations, from the settings. A phase keeps the length it started with.
struct Plan {
  uint8_t focusMin = 25, shortMin = 5, longMin = 15;
  uint8_t rounds = 4;      // focus sessions before the long break, 1-8
  bool autoNext = true;    // the next phase starts by itself
  uint32_t ms(Phase p) const {
    return (uint32_t)(p == Phase::Focus ? focusMin : p == Phase::Short ? shortMin : longMin) * 60000UL;
  }
};

class Core {
 public:
  Phase phase() const { return phase_; }
  Run run() const { return run_; }
  // Focus sessions finished in this set (0 to rounds; the long break shows all).
  uint8_t done() const { return done_; }

  uint32_t totalMs(const Plan& p) const { return run_ == Run::Idle ? p.ms(phase_) : total_; }
  uint32_t leftMs(uint32_t now, const Plan& p) const {
    if (run_ == Run::Idle) return p.ms(phase_);
    if (run_ == Run::Paused) return left_;
    const int32_t l = (int32_t)(end_ - now);
    return l > 0 ? (uint32_t)l : 0;
  }

  // Starts the phase shown, or resumes a paused one.
  void start(uint32_t now, const Plan& p) {
    if (run_ == Run::Paused) { end_ = now + left_; run_ = Run::Running; return; }
    if (run_ == Run::Running) return;
    total_ = p.ms(phase_);
    end_ = now + total_;
    run_ = Run::Running;
  }
  void pause(uint32_t now) {
    if (run_ != Run::Running) return;
    const int32_t l = (int32_t)(end_ - now);
    left_ = l > 0 ? (uint32_t)l : 0;
    run_ = Run::Paused;
  }
  // Back to the first focus session, stopped.
  void reset() { *this = Core(); }
  // Ends this phase now, counted as done: a running one goes straight on into
  // the next, a paused or stopped one leaves the next one stopped.
  void skip(uint32_t now, const Plan& p) { advance(now, p, run_ == Run::Running); }

  // Call every loop pass. True when the running phase ran out; `ended` is the
  // phase that did. The next one starts from the moment it ran out (so a late
  // pass does not stretch the cycle), unless that is long past.
  bool tick(uint32_t now, const Plan& p, Phase& ended) {
    if (run_ != Run::Running || (int32_t)(now - end_) < 0) return false;
    ended = phase_;
    const uint32_t at = now - end_ < 5000 ? end_ : now;
    advance(at, p, p.autoNext);
    return true;
  }

 private:
  void advance(uint32_t at, const Plan& p, bool go) {
    if (phase_ == Phase::Focus) {
      if (done_ < 255) ++done_;
      phase_ = done_ >= (p.rounds ? p.rounds : 1) ? Phase::Long : Phase::Short;
    } else {
      if (phase_ == Phase::Long) done_ = 0;
      phase_ = Phase::Focus;
    }
    run_ = Run::Idle;
    if (go) start(at, p);
  }

  Phase phase_ = Phase::Focus;
  Run run_ = Run::Idle;
  uint8_t done_ = 0;
  uint32_t total_ = 0, end_ = 0, left_ = 0;
};

}  // namespace pomo
