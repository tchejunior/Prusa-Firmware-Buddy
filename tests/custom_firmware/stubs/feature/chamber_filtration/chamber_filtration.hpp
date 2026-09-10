#pragma once
#include <feature/chamber_filtration/chamber_filtration_enums.hpp>
namespace buddy {
class ChamberFiltration {
public:
    ChamberFiltrationBackend backend() const;
};
ChamberFiltration &chamber_filtration();
} // namespace buddy
