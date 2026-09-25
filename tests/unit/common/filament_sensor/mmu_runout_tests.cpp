#include <feature/filament_sensor/mmu_runout.hpp>
#include <catch2/catch.hpp>

using State = FilamentSensorState;
using Action = MmuRunout::Action;

TEST_CASE("MMU runout consumes the Bowden tail before pausing") {
    MmuRunout runout;
    REQUIRE(runout.step(true, true, true, false, State::HasFilament, State::HasFilament, 2) == Action::none);
    REQUIRE(runout.step(true, true, true, true, State::NoFilament, State::HasFilament, 2) == Action::warn);
    REQUIRE(runout.slot() == 2);
    REQUIRE(runout.step(true, true, true, false, State::HasFilament, State::HasFilament, 2) == Action::none);
    REQUIRE(runout.step(true, true, true, false, State::NoFilament, State::NoFilament, 2) == Action::pause);
    REQUIRE(runout.step(true, true, true, false, State::NoFilament, State::NoFilament, 2) == Action::none);
}

TEST_CASE("Simultaneous FINDA and ADC runout pauses immediately") {
    MmuRunout runout;
    REQUIRE(runout.step(true, true, true, true, State::NoFilament, State::NoFilament, 0) == Action::pause);
}

TEST_CASE("Invalid ADC state fails safe after FINDA runout") {
    for (const auto state : { State::NotInitialized, State::NotCalibrated, State::NotConnected, State::Disabled }) {
        MmuRunout runout;
        REQUIRE(runout.step(true, true, true, true, State::NoFilament, State::HasFilament, 0) == Action::warn);
        REQUIRE(runout.step(true, true, true, false, State::NoFilament, state, 0) == Action::pause);
    }
}

TEST_CASE("Locks delay handling and sensor levels recover missed edges") {
    MmuRunout runout;
    REQUIRE(runout.step(true, true, false, true, State::NoFilament, State::HasFilament, 4) == Action::none);
    REQUIRE_FALSE(runout.slot());
    REQUIRE(runout.step(true, true, true, false, State::NoFilament, State::HasFilament, 4) == Action::warn);
    REQUIRE(runout.step(true, true, false, false, State::NoFilament, State::NoFilament, 4) == Action::none);
    REQUIRE(runout.step(true, true, true, false, State::NoFilament, State::NoFilament, 4) == Action::pause);
}

TEST_CASE("Non-MMU operation and invalid slots never arm recovery") {
    MmuRunout runout;
    REQUIRE(runout.step(false, true, true, true, State::NoFilament, State::HasFilament, 0) == Action::none);
    REQUIRE(runout.step(true, true, true, true, State::NoFilament, State::HasFilament, 255) == Action::none);
    REQUIRE_FALSE(runout.slot());
}

TEST_CASE("Print end clears a pending runout") {
    MmuRunout runout;
    REQUIRE(runout.step(true, true, true, true, State::NoFilament, State::HasFilament, 1) == Action::warn);
    REQUIRE(runout.step(true, false, false, false, State::NoFilament, State::HasFilament, 1) == Action::none);
    REQUIRE_FALSE(runout.slot());
}

TEST_CASE("Power panic restore keeps the slot and requires recovery") {
    MmuRunout runout;
    runout.restore(3);
    REQUIRE(runout.slot() == 3);
    REQUIRE(runout.step(true, true, true, false, State::HasFilament, State::HasFilament, 255) == Action::pause);
    REQUIRE(runout.step(true, true, true, false, State::HasFilament, State::HasFilament, 255) == Action::none);

    runout.restore(255);
    REQUIRE_FALSE(runout.slot());
}
