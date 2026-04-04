#pragma once

#include <chrono>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <stdexcept>

class CTestNamedPipeReadySignal final
{
public:
    void NotifyFailure(std::exception_ptr failure)
    {
        {
            std::lock_guard lock(m_mutex);
            m_failure = std::move(failure);
            m_completed = true;
        }

        m_conditionVariable.notify_all();
    }

    void NotifyReady()
    {
        {
            std::lock_guard lock(m_mutex);
            m_completed = true;
        }

        m_conditionVariable.notify_all();
    }

    void Wait(const std::chrono::milliseconds timeout = std::chrono::seconds(2))
    {
        std::unique_lock lock(m_mutex);

        if (!m_conditionVariable.wait_for(lock, timeout, [this] { return m_completed; }))
        {
            throw std::runtime_error("Timed out while waiting for named-pipe test readiness.");
        }

        if (m_failure != nullptr)
        {
            std::rethrow_exception(m_failure);
        }
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_conditionVariable;
    std::exception_ptr m_failure;
    bool m_completed = false;
};
