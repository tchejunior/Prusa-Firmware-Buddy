#pragma once

namespace marlin_server {

// A successful replacement consumes the pre-load retraction. Only the signed
// post-purge retraction remains; ordinary changes retain their local E origin.
constexpr float filament_change_resume_e(float local_resume_e, float print_resume_e,
    float final_retract, bool interrupted, bool replaced) {
    return (interrupted && replaced ? print_resume_e : local_resume_e)
        + (interrupted ? final_retract : 0.f);
}

} // namespace marlin_server
