#pragma once

#include "filament_sensor_states.hpp"

#include <atomic>
#include <optional>
#include <stdint.h>

/// State for the CORE One MMU runout policy. FINDA identifies the exhausted
/// slot, while the extruder ADC decides when the printable tail is consumed.
class MmuRunout {
public:
    enum class Action {
        none,
        warn,
        pause,
    };

    Action step(bool enabled, bool print_active, bool can_run, bool finda_removed,
        FilamentSensorState finda, FilamentSensorState extruder, uint8_t slot) {
        if (!enabled || !print_active) {
            reset();
            return Action::none;
        }
        auto state = state_.load(std::memory_order_acquire);
        if (!can_run || (state & pause_requested_bit)) {
            return Action::none;
        }

        if (state == idle && (finda_removed || finda == FilamentSensorState::NoFilament) && slot < slot_count) {
            const uint8_t armed = encode_slot(slot);
            if (!state_.compare_exchange_strong(state, armed, std::memory_order_acq_rel)) {
                return Action::none;
            }
            state = armed;
            if (extruder == FilamentSensorState::HasFilament) {
                return Action::warn;
            }
        }

        if (decode_slot(state) && ((state & require_pause_bit) || extruder != FilamentSensorState::HasFilament || !is_fsensor_working_state(finda))) {
            const uint8_t paused = state | pause_requested_bit;
            if (state_.compare_exchange_strong(state, paused, std::memory_order_acq_rel)) {
                return Action::pause;
            }
        }
        return Action::none;
    }

    std::optional<uint8_t> slot() const {
        return decode_slot(state_.load(std::memory_order_acquire));
    }

    void reset() {
        state_.store(idle, std::memory_order_release);
    }

    void restore(uint8_t slot) {
        state_.store(slot < slot_count ? encode_slot(slot) | require_pause_bit : idle, std::memory_order_release);
    }

private:
    static constexpr uint8_t idle = 0;
    static constexpr uint8_t slot_count = 5;
    static constexpr uint8_t require_pause_bit = 0x40;
    static constexpr uint8_t pause_requested_bit = 0x80;

    static constexpr uint8_t encode_slot(uint8_t slot) { return slot + 1; }
    static constexpr std::optional<uint8_t> decode_slot(uint8_t state) {
        const uint8_t encoded = state & ~(require_pause_bit | pause_requested_bit);
        return encoded >= 1 && encoded <= slot_count ? std::optional<uint8_t> { static_cast<uint8_t>(encoded - 1) } : std::nullopt;
    }

    // Measurement owns transitions; Marlin may inspect, restore or reset them.
    std::atomic<uint8_t> state_ { idle };
};
