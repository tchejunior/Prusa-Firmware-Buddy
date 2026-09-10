#pragma once
#include <stdexcept>
[[noreturn]] inline void bsod_unreachable() { throw std::logic_error("Unexpected filtration backend"); }
