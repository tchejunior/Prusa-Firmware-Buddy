#include <feature/xbuddy_extension/cooling.hpp>
#include <feature/chamber_filtration/chamber_filtration_enums.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace buddy;

namespace buddy {
extern ChamberFiltrationBackend test_filtration_backend;
}

TEST_CASE("Custom filtration retains unfiltered rear fan regulation and thermal protection") {
    test_filtration_backend = ChamberFiltrationBackend::xbe_custom_filter;
    FanCooling custom;
    custom.regulator_legacy = false;
    REQUIRE(custom.compute_pwm_step(40, 55, pwm_auto, PWM255 { 255 }).value == 0);
    const auto filtering = custom.compute_custom_filtration_step(40, 55, pwm_auto, pwm_auto, PWM255 { 255 }, PWM255 { 180 });
    REQUIRE(filtering.cooling.value == 0);
    REQUIRE(filtering.filtration.value == 180);
    const auto cooling_only = custom.compute_custom_filtration_step(54, 50, pwm_auto, PWM255 { 0 }, PWM255 { 255 }, PWM255 { 180 });
    REQUIRE(cooling_only.cooling.value == 40);
    REQUIRE(cooling_only.filtration.value == 0);
    // Four degrees over target -> 40 PWM, without the 2x/3x exhaust-filter multiplier.
    REQUIRE(custom.compute_pwm_step(54, 50, pwm_auto, PWM255 { 255 }).value == 40);
    REQUIRE(custom.compute_pwm_step(54, 50, PWM255 { 80 }, PWM255 { 255 }).value == 80);
    static_cast<void>(custom.compute_pwm_step(FanCooling::overheating_temp, 50, PWM255 { 0 }, PWM255 { 255 }));
    REQUIRE(custom.apply_pwm_overrides(true, PWM255 { 0 }).value == 255);
    test_filtration_backend = ChamberFiltrationBackend::none;
}

std::ostream &operator<<(std::ostream &os, const FanCooling::FanPWM &pwm) {
    return os << "FanPWM{" << static_cast<int>(pwm.value) << "}";
}

TEST_CASE("Cooling PWM") {
    buddy::FanCooling cooling;
    static constexpr buddy::FanCooling::FanPWM max_auto_pwm { 100 };

    const auto step = [&](bool already_spinning, Temperature current_temperature, std::optional<Temperature> target_temperature, PWM255OrAuto target_pwm) {
        auto result = cooling.compute_pwm_step(current_temperature, target_temperature, target_pwm, max_auto_pwm);
        result = cooling.apply_pwm_overrides(already_spinning, result);
        return result;
    };

    SECTION("Manual, full pwm") {
        std::optional<Temperature> target_temperature;
        SECTION("With target temp") {
            target_temperature = 60;
        }

        SECTION("Without target temp") {}

        REQUIRE(step(true, 54, target_temperature, cooling.max_pwm) == cooling.max_pwm);
        REQUIRE(step(false, 54, target_temperature, cooling.max_pwm) == cooling.max_pwm);
    }

    SECTION("Manual, low PWM") {
        const PWM255 target_pwm { 5 };

        SECTION("Already running") {
            REQUIRE(step(true, 54, std::nullopt, target_pwm) == cooling.min_pwm);
        }

        SECTION("Initial kick") {
            REQUIRE(step(false, 54, std::nullopt, target_pwm) == cooling.spin_up_pwm);
        }
    }

    SECTION("Auto cooling, no target temp") {
        REQUIRE(step(true, 45, std::nullopt, pwm_auto).value == 0);
    }

    SECTION("Auto cooling, cold chamber") {
        REQUIRE(step(false, 20, 60, pwm_auto).value == 0);
    }

    SECTION("Auto cooling, slightly cool") {
        SECTION("Not running, don't start") {
            REQUIRE(step(false, 59, 60, pwm_auto).value == 0);
        }
    }

    SECTION("Auto cooling, really hot") {
        const std::optional<Temperature> target_temperature = 20;
        const Temperature current_temperature = 55;

        const auto result = step(true, current_temperature, target_temperature, pwm_auto);
        REQUIRE(result > PWM255 { 0 });
        for (uint32_t i = 0; i < 10; i++) {
            step(true, current_temperature, target_temperature, pwm_auto);
        }
        REQUIRE(step(true, current_temperature, target_temperature, pwm_auto) == max_auto_pwm);
    }

    SECTION("Nonsense range test") {
        const Temperature current_temperature = 55;

        REQUIRE(step(true, current_temperature, -100, pwm_auto) == max_auto_pwm);

        // due to previous regulation cycle, the target value must be multiplied
        REQUIRE(step(true, current_temperature, 300, pwm_auto) == PWM255 { 0 });
    }

    SECTION("Fan kick up speed") {
        Temperature target_temperature = 20;

        // Use a small error that gets clamped to min_pwm
        // Error of 4°C * ramp_slope(10) = 40 PWM, which equals min_pwm after apply_pwm_overrides
        REQUIRE(step(false, 4.0f + target_temperature, target_temperature, pwm_auto) == cooling.spin_up_pwm);

        REQUIRE(step(true, 4.0f + target_temperature, target_temperature, pwm_auto) == cooling.min_pwm);
    }

    SECTION("Overheating cooling") {
        SECTION("Full power") {
            const Temperature target_temperature = 20;
            REQUIRE(step(true, cooling.overheating_temp, target_temperature, pwm_auto) == cooling.max_pwm);
            REQUIRE(cooling.get_overheating_temp_flag());
            REQUIRE(step(true, cooling.recovery_temp + 1.0, target_temperature, pwm_auto) == cooling.max_pwm);

            step(true, cooling.recovery_temp - 1.0, target_temperature, pwm_auto); // one more regulation cycle is needed to recover
            REQUIRE(step(true, cooling.recovery_temp - 1.0, target_temperature, pwm_auto) == max_auto_pwm);

            REQUIRE(step(true, cooling.critical_temp, target_temperature, pwm_auto) == cooling.max_pwm);
            REQUIRE(cooling.get_critical_temp_flag());

            REQUIRE(step(true, cooling.recovery_temp + 1.0, target_temperature, pwm_auto) == cooling.max_pwm);
            step(true, cooling.recovery_temp - 1.0, target_temperature, pwm_auto); // one regulation cycle is needed to recover
            REQUIRE(step(true, cooling.recovery_temp - 1.0, target_temperature, pwm_auto) == max_auto_pwm);
        }
    }
}
