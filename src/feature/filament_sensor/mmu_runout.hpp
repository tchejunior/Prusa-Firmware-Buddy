#pragma once

#include "filament_sensor_states.hpp"
#include <optional>

/// Marlin-task-owned state. A FINDA runout is latched until recovery or print end;
/// inserting another filament into FINDA must not hide the loose tail in the tube.
class MmuRunout {
public:
    enum class Action { none,
        warn,
        pause };

    Action step(bool enabled, bool print_active, bool can_run, bool finda_removed,
        FilamentSensorState finda, FilamentSensorState extruder, uint8_t slot) {
        if (!enabled || !print_active) {
            reset();
            return Action::none;
        }
        if (!can_run || pause_requested_) {
            return Action::none;
        }
        if (!slot_ && (finda_removed || finda == FilamentSensorState::NoFilament) && slot < 5) {
            slot_ = slot;
            // Level detection reconstructs a pending tail after a pause, lock or reboot.
            // Both sensors can go empty in the same iteration. Do not lose the
            // ADC event while issuing the early warning.
            if (extruder == FilamentSensorState::HasFilament) {
                return Action::warn;
            }
        }
        if (slot_ && (require_pause_ || extruder != FilamentSensorState::HasFilament || !is_fsensor_working_state(finda))) {
            pause_requested_ = true;
            return Action::pause;
        }
        return Action::none;
    }

    std::optional<uint8_t> slot() const { return slot_; }
    void reset() {
        slot_.reset();
        pause_requested_ = false;
        require_pause_ = false;
    }
    void restore(uint8_t slot) {
        reset();
        if (slot < 5) {
            slot_ = slot;
            require_pause_ = true;
        }
    }

private:
    std::optional<uint8_t> slot_;
    bool pause_requested_ = false;
    bool require_pause_ = false;
};
