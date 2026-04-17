#include "TaskManager.hpp"
#include <algorithm>
#include <Windows.h>
TaskManager::TaskManager(HWND hwndNotify)
    : m_hwndNotify(hwndNotify)
{
}

TaskManager::~TaskManager() {
    stop_all();
}

void TaskManager::add_task(std::shared_ptr<Task> task) {
    {
        std::lock_guard<std::mutex> lg(m_mutex);
        m_tasks.push_back(task);
    }
    // no notify yet
}

void TaskManager::start_all() {
    std::lock_guard<std::mutex> lg(m_mutex);
    for (auto& t : m_tasks) {
        // Start each task; they will run independently and notify UI via PostMessage when they change.
        // To make tasks notify UI, wrap the task function to PostMessage on state transitions.
        // But because Task's fn is provided externally, we assume tasks call notify via lambda capture.
        t->start();
    }
    // Kick a UI update
    notify_ui();
}

void TaskManager::stop_all() {
    std::lock_guard<std::mutex> lg(m_mutex);
    for (auto& t : m_tasks) {
        t->request_stop();
    }
    notify_ui();
}

std::vector<std::pair<std::string, TaskState>> TaskManager::snapshot() {
    std::vector<std::pair<std::string, TaskState>> out;
    std::lock_guard<std::mutex> lg(m_mutex);
    for (auto& t : m_tasks) {
        out.emplace_back(t->get_name(), t->get_state());
    }
    return out;
}

void TaskManager::notify_ui() {
    if (m_hwndNotify && ::IsWindow(m_hwndNotify)) {
        // post, not send, so worker thread doesn't block
        ::PostMessage(m_hwndNotify, WM_TASK_UPDATE, 0, 0);
    }
}



