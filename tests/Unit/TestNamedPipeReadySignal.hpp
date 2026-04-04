#pragma once

#include <condition_variable>
#include <mutex>

class CTestNamedPipeReadySignal final
{
public:
    void NotifyReady()
    {
        {
            std::lock_guard lock(m_mutex);
            m_ready = true;
        }

        m_conditionVariable.notify_all();
    }

    void Wait()
    {
        std::unique_lock lock(m_mutex);
        m_conditionVariable.wait(lock, [this] { return m_ready; });
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_conditionVariable;
    bool m_ready = false;
};
