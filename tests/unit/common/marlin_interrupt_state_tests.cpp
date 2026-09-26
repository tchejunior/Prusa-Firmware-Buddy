#include <catch2/catch.hpp>
#include <marlin_server_types/marlin_server_state.h>
#include <gcode/filament_change_resume.hpp>

using namespace marlin_server;

TEST_CASE("Replacement repays only its final retraction") {
    // E=100 at interruption, E=98 after server retract, final retract=-2.
    const float end_e = filament_change_resume_e(98, 100, -2, true, true);
    REQUIRE(100 - end_e == Approx(2));
    REQUIRE(filament_change_resume_e(98, 100, 0, true, true) == Approx(100));
    REQUIRE(filament_change_resume_e(-52, -50, -2, true, true) == Approx(-52));
    REQUIRE(filament_change_resume_e(98, 100, -2, false, true) == Approx(98));
    REQUIRE(filament_change_resume_e(98, 100, -2, true, false) == Approx(96));
}

TEST_CASE("Filament interruption remains in the paused resume sequence") {
    const auto state = State::Resuming_ExecutingGCodeInterrupt;
    REQUIRE(is_resuming_state(state));
    REQUIRE(is_extended_paused_state(state));
    REQUIRE_FALSE(is_printing_state(state));
    REQUIRE_FALSE(is_abort_state(state));
}

TEST_CASE("Abort during an interruption cannot be classified as resuming") {
    for (auto state : { State::Aborting_Begin, State::Aborting_WaitIdle, State::Aborted }) {
        REQUIRE(is_abort_state(state));
        REQUIRE_FALSE(is_resuming_state(state));
        REQUIRE_FALSE(is_extended_paused_state(state));
    }
}
