#ifndef REMOTE_REPLAY_WINDOW_H_
#define REMOTE_REPLAY_WINDOW_H_

#include <cstdint>

namespace REMOTE_REPLAY {

constexpr uint64_t kWindowBits = 64;

struct State {
    uint64_t highest = 0;
    uint64_t seen = 0;
};

// Marks a counter as consumed. Counters may arrive out of order inside the
// latest 64-value window, but each value is accepted at most once.
inline bool consume(State& state, uint64_t counter) {
    if (counter == 0) return false;
    if (state.highest == 0) {
        state.highest = counter;
        state.seen = 1;
        return true;
    }
    if (counter > state.highest) {
        const uint64_t advance = counter - state.highest;
        state.seen = advance >= kWindowBits ? 1 : (state.seen << advance) | 1;
        state.highest = counter;
        return true;
    }

    const uint64_t age = state.highest - counter;
    if (age >= kWindowBits) return false;
    const uint64_t bit = uint64_t{1} << age;
    if ((state.seen & bit) != 0) return false;
    state.seen |= bit;
    return true;
}

} // namespace REMOTE_REPLAY

#endif
