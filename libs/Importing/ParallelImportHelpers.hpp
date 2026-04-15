#pragma once

#include <semaphore>
#include <string_view>

#include "Domain/ServiceContracts.hpp"

namespace Librova::Importing {

// Null progress sink — used inside parallel worker lambdas where no progress
// reporting is needed (workers update the main thread's sink indirectly).
class CNullProgressSink final : public Librova::Domain::IProgressSink
{
public:
    void ReportValue(int, std::string_view) override {}
    [[nodiscard]] bool IsCancellationRequested() const override { return false; }
};

// RAII guard that releases a counting_semaphore on destruction.
// Pair with an explicit acquire() at the call site.
template <std::ptrdiff_t kLeast>
struct SSemRelease
{
    std::counting_semaphore<kLeast>& Sem;
    ~SSemRelease() noexcept { Sem.release(); }
};

} // namespace Librova::Importing
