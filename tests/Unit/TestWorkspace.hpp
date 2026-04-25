#pragma once

#include <atomic>
#include <filesystem>
#include <string>

#define NOMINMAX
#include <Windows.h>

// Generates a unique temp path: <temp>/<prefix>.<pid>.<counter>
// The path is not created on disk.
[[nodiscard]] inline std::filesystem::path MakeUniqueTestPath(const wchar_t* prefix)
{
    static std::atomic<unsigned long long> s_counter{0};
    const auto pid = static_cast<unsigned long long>(GetCurrentProcessId());
    const auto seq = s_counter.fetch_add(1, std::memory_order_relaxed);
    const std::wstring name = std::wstring(prefix) + L"." + std::to_wstring(pid) + L"." + std::to_wstring(seq);
    return std::filesystem::temp_directory_path() / std::filesystem::path{name};
}

// RAII unique sandbox directory.
// Creates a fresh directory on construction, removes it (and all contents) on destruction.
class CTestWorkspace final
{
public:
    explicit CTestWorkspace(const wchar_t* prefix)
        : m_path(MakeUniqueTestPath(prefix))
    {
        std::filesystem::remove_all(m_path);
        std::filesystem::create_directories(m_path);
    }

    ~CTestWorkspace()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }

    CTestWorkspace(const CTestWorkspace&) = delete;
    CTestWorkspace& operator=(const CTestWorkspace&) = delete;

    [[nodiscard]] const std::filesystem::path& GetPath() const noexcept { return m_path; }

private:
    std::filesystem::path m_path;
};
