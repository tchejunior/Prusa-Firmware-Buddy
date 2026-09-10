#include <feature/filament_sensor/mmu_runout.hpp>
#include <catch2/catch_test_macros.hpp>

using State = FilamentSensorState;
using Action = MmuRunout::Action;

TEST_CASE("MMU runout consumes the tube before requesting a pause") {
    MmuRunout runout;
    REQUIRE(runout.step(true, true, true, false, State::HasFilament, State::HasFilament, 2) == Action::none);
    REQUIRE(runout.step(true, true, true, true, State::NoFilament, State::HasFilament, 2) == Action::warn);
    REQUIRE(runout.slot() == 2);
    // Flicker, or a replacement inserted early, cannot cancel the loose tail.
    REQUIRE(runout.step(true, true, true, false, State::HasFilament, State::HasFilament, 2) == Action::none);
    REQUIRE(runout.step(true, true, true, true, State::NoFilament, State::HasFilament, 2) == Action::none);
    REQUIRE(runout.step(true, true, true, false, State::NoFilament, State::NoFilament, 2) == Action::pause);
    REQUIRE(runout.step(true, true, true, false, State::NoFilament, State::NoFilament, 2) == Action::none);
    runout.reset();
    REQUIRE_FALSE(runout.slot());
    REQUIRE(runout.step(true, true, true, true, State::NoFilament, State::HasFilament, 3) == Action::warn);
    REQUIRE(runout.slot() == 3);
}

TEST_CASE("Simultaneous MMU and ADC runout is not lost") {
    MmuRunout runout;
    REQUIRE(runout.step(true, true, true, true, State::NoFilament, State::NoFilament, 0) == Action::pause);
}

TEST_CASE("Invalid ADC stops printing instead of pretending it still has filament") {
    for (const auto state : { State::NotInitialized, State::NotCalibrated, State::NotConnected, State::Disabled }) {
        MmuRunout runout;
        REQUIRE(runout.step(true, true, true, true, State::NoFilament, State::HasFilament, 0) == Action::warn);
        REQUIRE(runout.step(true, true, true, false, State::NoFilament, state, 0) == Action::pause);
    }
}

TEST_CASE("MMU runout waits across pause and event locks; print end clears it") {
    MmuRunout runout;
    REQUIRE(runout.step(true, true, true, true, State::NoFilament, State::HasFilament, 1) == Action::warn);
    REQUIRE(runout.step(true, true, false, false, State::NoFilament, State::NoFilament, 1) == Action::none);
    REQUIRE(runout.slot() == 1);
    REQUIRE(runout.step(true, true, true, false, State::NoFilament, State::NoFilament, 1) == Action::pause);
    REQUIRE(runout.step(true, false, false, false, State::NoFilament, State::NoFilament, 1) == Action::none);
    REQUIRE_FALSE(runout.slot());
}

TEST_CASE("Ordinary loading and disabled MMU do not arm runout") {
    MmuRunout runout;
    REQUIRE(runout.step(true, true, false, true, State::NoFilament, State::HasFilament, 0) == Action::none);
    REQUIRE(runout.step(false, true, true, true, State::NoFilament, State::HasFilament, 0) == Action::none);
    REQUIRE(runout.step(true, true, true, true, State::NoFilament, State::HasFilament, 255) == Action::none);
    REQUIRE_FALSE(runout.slot());
}

TEST_CASE("A FINDA edge during an event lock is recovered from sensor level") {
    MmuRunout runout;
    REQUIRE(runout.step(true, true, false, true, State::NoFilament, State::HasFilament, 4) == Action::none);
    REQUIRE_FALSE(runout.slot());
    REQUIRE(runout.step(true, true, true, false, State::NoFilament, State::HasFilament, 4) == Action::warn);
    REQUIRE(runout.slot() == 4);
    REQUIRE(runout.step(true, true, true, false, State::NoFilament, State::NoFilament, 4) == Action::pause);
}

TEST_CASE("Restored runout waits for readiness then pauses even with no current MMU tool") {
    MmuRunout runout;
    runout.restore(3);
    // The caller disables can_run while power panic awaits confirmation and
    // restores coordinates/heaters. A partial load may cover both sensors.
    REQUIRE(runout.step(true, true, false, false, State::HasFilament, State::HasFilament, 255) == Action::none);
    REQUIRE(runout.slot() == 3);
    REQUIRE(runout.step(true, true, false, false, State::HasFilament, State::HasFilament, 255) == Action::none);
    REQUIRE(runout.step(true, true, true, false, State::HasFilament, State::HasFilament, 255) == Action::pause);
    REQUIRE(runout.slot() == 3);
    REQUIRE(runout.step(true, true, true, false, State::HasFilament, State::HasFilament, 255) == Action::none);
    runout.reset();
    REQUIRE(runout.step(true, true, true, false, State::HasFilament, State::HasFilament, 3) == Action::none);
}

TEST_CASE("Invalid stored slots are discarded and a FINDA fault cannot hide a pending tail") {
    MmuRunout runout;
    for (const uint8_t slot : { 5, 254, 255 }) {
        runout.restore(slot);
        REQUIRE_FALSE(runout.slot());
    }
    REQUIRE(runout.step(true, true, true, false, State::NoFilament, State::HasFilament, 0) == Action::warn);
    REQUIRE(runout.step(true, true, true, false, State::NotConnected, State::HasFilament, 0) == Action::pause);
}
