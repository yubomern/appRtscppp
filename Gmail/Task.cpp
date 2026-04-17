#include "Task.hpp"
#include <iostream>
#include <chrono>
#include <exception>

Task::Task(std::string name, TaskFn fn)
    : m_name(std::move(name))
    , m_fn(fn)
    , m_state(TaskState::Ready)
    , m_stopFlag(false)
{
}

Task::~Task() {
    request_stop();
    // join if thread is joinable or future exists
    if (m_thread.joinable()) {
        try { m_thread.join(); }
        catch (...) {}
    }
    if (m_future.valid()) {
        try { m_future.wait(); }
        catch (...) {}
    }
}

void Task::start() {
    TaskState expected = TaskState::Ready;
    if (!m_state.compare_exchange_strong(expected, TaskState::Running)) {
        // already running or finished
        return;
    }

    // Demonstrate two patterns:
    // If name contains "async" we use std::async which inside launches a std::thread to show nested usage.
    if (m_name.find("async") != std::string::npos) {
        // start as async and capture this pointer safely
        m_future = std::async(std::launch::async, [this]() {
            run_wrapper();
            });
    }
    else {
        // start as normal thread
        m_thread = std::thread([this]() {
            run_wrapper();
            });
    }
}

void Task::run_wrapper() {
    try {
        // Example: inside this wrapper we'll show an async starting a thread
        // if the task wants to do some sub-work concurrently.
        // Keep this general: call the provided function with stopFlag.
        m_fn(m_stopFlag);
        if (m_stopFlag.load()) {
            m_state.store(TaskState::Stopped);
        }
        else {
            m_state.store(TaskState::Finished);
        }
    }
    catch (const std::exception& e) {
        (void)e;
        m_state.store(TaskState::Error);
    }
    catch (...) {
        m_state.store(TaskState::Error);
    }
}

void Task::request_stop() {
    m_stopFlag.store(true);
}

void Task::force_stop() {
    // We cannot forcibly kill the thread; we only set stop flag.
    request_stop();
}

TaskState Task::get_state()const  {
    return m_state.load();
}

std::string Task::get_name() const  {
    return m_name;
}

void Task::join() {
    std::lock_guard<std::mutex> lg(m_joinMutex);
    if (m_thread.joinable()) m_thread.join();
    if (m_future.valid()) m_future.wait();
}

std::future<void> Task::get_future() {
    return std::move(m_future);
}
