#include <cstdlib>
#include <iostream>

#include "remote_replay_window.h"

namespace {

void expect(const char* name, bool actual, bool wanted) {
    if (actual == wanted) return;
    std::cerr << name << ": expected " << wanted << ", got " << actual << '\n';
    std::exit(1);
}

} // namespace

int main() {
    REMOTE_REPLAY::State state;
    expect("zero", REMOTE_REPLAY::consume(state, 0), false);
    expect("first", REMOTE_REPLAY::consume(state, 5), true);
    expect("duplicate first", REMOTE_REPLAY::consume(state, 5), false);
    expect("advance", REMOTE_REPLAY::consume(state, 8), true);
    expect("delayed seven", REMOTE_REPLAY::consume(state, 7), true);
    expect("duplicate seven", REMOTE_REPLAY::consume(state, 7), false);
    expect("delayed six", REMOTE_REPLAY::consume(state, 6), true);
    expect("advance to seventy", REMOTE_REPLAY::consume(state, 70), true);
    expect("edge inside window", REMOTE_REPLAY::consume(state, 9), true);
    expect("too old", REMOTE_REPLAY::consume(state, 6), false);
    expect("large jump", REMOTE_REPLAY::consume(state, 1000), true);
    expect("forgotten old value", REMOTE_REPLAY::consume(state, 70), false);
    expect("recent unseen", REMOTE_REPLAY::consume(state, 999), true);
    expect("recent duplicate", REMOTE_REPLAY::consume(state, 999), false);
    return 0;
}
