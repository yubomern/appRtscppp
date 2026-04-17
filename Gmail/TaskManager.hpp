#pragma once
#include "Task.hpp"
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <windows.h> // for HWND

// Message posted to the UI when a task updates its state
constexpr UINT WM_TASK_UPDATE = WM_USER + 100;

class TaskManager {
public:
    TaskManager(HWND hwndNotify);
    ~TaskManager();

    // create+add task
    void add_task(std::shared_ptr<Task> task);

    // start all tasks (each starts independently)
    void start_all();

    // request stop on all tasks
    void stop_all();

    // get snapshot of task states (thread-safe)
    std::vector<std::pair<std::string, TaskState>> snapshot();

private:
    HWND m_hwndNotify; // window to PostMessage to for updates
    std::vector<std::shared_ptr<Task>> m_tasks;
    std::mutex m_mutex;

    // Helper: when a task changes state, we PostMessage to UI thread
    void notify_ui();
};
