// Stub for chamber_filtration for unit testing
#include <feature/chamber_filtration/chamber_filtration.hpp>

namespace buddy {

ChamberFiltrationBackend test_filtration_backend = ChamberFiltrationBackend::none;

ChamberFiltration &chamber_filtration() {
    static ChamberFiltration instance;
    return instance;
}

ChamberFiltrationBackend ChamberFiltration::backend() const {
    return test_filtration_backend;
}

} // namespace buddy
