#pragma once

#include <cmath>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <vector>

template <typename T>
class ConcurentQueue
{
public:
    inline void notify_all()
    {
        m_cv.notify_all();
    }

    void push(T&& item)
    {
        {
            auto guard = std::lock_guard(m_lock);

            m_queue.push_back(std::move(item));
        }
        m_cv.notify_one();
    }

    void push(const T& item)
    {
        {
            auto guard = std::lock_guard(m_lock);

            m_queue.push_back(item);
        }
        m_cv.notify_one();
    }

    std::optional<T> pop()
    {
        auto guard = std::lock_guard(m_lock);

        if (m_queue.size() == 0) return std::nullopt;

        auto ret = std::move(m_queue.front());

        m_queue.pop_front();

        return ret;
    }

    //appends elements to the back of the given vec. returns how many elements are appended
    uint32_t fetch_available(std::vector<T>& pushed_vec, uint32_t max_fetch)
    {

        auto guard = std::lock_guard(m_lock);

        uint32_t fetch_count = std::min(max_fetch, (uint32_t)m_queue.size());

        if (fetch_count == 0) return 0;

        pushed_vec.reserve(pushed_vec.size() + fetch_count);

        for (uint32_t i = 0; i < fetch_count; ++i)
        {
            pushed_vec.push_back(std::move(m_queue[i]));
        }

        m_queue.erase(m_queue.begin(), m_queue.begin() + fetch_count);

        return fetch_count;
    }

    uint32_t fetch_some_blocking(std::vector<T>& pushed_vec, uint32_t max_fetch)
    {

        if (auto ret = fetch_available(pushed_vec, max_fetch); ret != 0) return ret;

        auto guard = std::unique_lock(m_lock);
        m_cv.wait(guard);

        uint32_t fetch_count = std::min(max_fetch, (uint32_t)m_queue.size());

        if (fetch_count == 0) return 0;

        pushed_vec.reserve(pushed_vec.size() + fetch_count);

        for (uint32_t i = 0; i < fetch_count; ++i)
        {
            pushed_vec.push_back(std::move(m_queue[i]));
        }

        m_queue.erase(m_queue.begin(), m_queue.begin() + fetch_count);

        return fetch_count;
    }

    void push(std::vector<T> pushed_vec)
    {
        if (pushed_vec.size())
        {
            auto guard = std::lock_guard(m_lock);

            for (auto& item : pushed_vec)
            {
                m_queue.push_back(std::move(item));
            }

            // m_queue.insert(m_queue.end(), pushed_vec.begin(), pushed_vec.end());
        }
        m_cv.notify_all();
    }

private:
    std::mutex m_lock;
    std::condition_variable m_cv;
    std::deque<T> m_queue;
};