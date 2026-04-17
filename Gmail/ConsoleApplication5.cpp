// A single-file Win32 + app wiring. Linker: user32.lib gdi32.lib

#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <chrono>
#include <thread>
#include <functional>
#include <algorithm>
#include "UDPapp.h"
#include <atomic>
#include <array>
#include <iostream>
#include "Task.hpp"
#include <boost/asio.hpp>
#include "TaskManager.hpp"
#include <condition_variable>
#include <Windows.h>

#include "GrH.h"
#pragma  comment (lib, "Ws2_32.lib")
#pragma  comment (lib, "gdi32.lib")
#pragma  comment (lib, "user32.lib")
static const wchar_t* CLASS_NAME = L"MultithreadStatusWindow";
static HWND g_hwnd = nullptr;
static std::unique_ptr<TaskManager> g_taskManager;

using namespace boost::asio;
using namespace boost::asio::ip;


std::atomic <bool> data_read_ = false;







enum class AppSsh {
    SSHCONNECT  , 
    SSHLOGIN 
};

class ILogger {

public :

    virtual void logger(const std::string& msg)const = 0 ;
    virtual void logger(const std::string& msg, int _a) = 0;
    virtual void logger(const std::string&& nameMsg) = 0;
    virtual ~ILogger()  {}
};

class ConsoleLogger : public  ILogger {

private  :
    std::string _nameMsg;

public :
    void logger(const std::string& msg) const override {
        std::cout << msg << std::endl;
    }



};






typedef struct Data Data;
struct Data {
    int value;
    Data(int v) : value(v) {}
    ~Data() { std::cout << "Destroy " << value << std::endl; }
};

std::mutex _mtx;
std::condition_variable cv_;

// ✅ Use normal shared_ptr — atomic ops via free functions
static std::shared_ptr<Data> aDtata;

enum class TData { UP, DOWN, RIGHT, LEF };
static Data astdata{ Data(0) };

void data_create(int v, int _f) {
    std::atomic_store(&aDtata, std::make_shared<Data>(v));
}

void data_vect_create(std::vector<int> vect, size_t _n, int acc = 0) {
    for (auto vc : vect)
        data_create(vc, 0);

    std::thread t([_n]() {
        auto current_data = std::atomic_load(&aDtata);
        if (current_data)
            std::cout << "T1 current: " << current_data->value << std::endl;
        std::atomic_store(&aDtata, std::make_shared<Data>(static_cast<int>(_n)));
        });

    std::thread t2([&]() {
        auto curr = std::atomic_load(&aDtata);
        if (curr)
            std::cout << "T2 current: " << curr->value << std::endl;
        });

    auto expected = std::atomic_load(&aDtata);
    auto g = std::make_shared<Data>(30);
    if (std::atomic_compare_exchange_strong(&aDtata, &expected, g))
        std::cout << "Thread 2 successfully exchanged.\n";
    else
        std::cout << "Thread 2 failed exchange.\n";

    t.join();
    t2.join();

    auto d = std::atomic_load(&aDtata);
    if (d) std::cout << "final data " << d->value << std::endl;
}

void python_task_runner(std::atomic<bool>& stopFlag, const  std::string& cmd)
{


    FILE* pipe = _popen(cmd.c_str(), "r");
    std::ofstream _oof("python_data.log", std::ios::app);
    int flag_ = 0;
    if (!_oof) {
        flag_ = -1;
        std::cerr << "errro opening file ppend";
    }

    if (!pipe) {
        std::cerr << "error lancer  task0  << " << std::endl;
        return;
    }
    std::array <char, 255> buffer{};

    while (fgets(buffer.data(), buffer.size(), pipe)) {
        if (stopFlag.load()) break;

        std::cout << "[PYTHON] " << buffer.data();
        if (flag_ == 0)
            _oof << "[PYTHON] " << buffer.data() << "\n";

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    }

    _pclose(pipe);

}

#include <map>

std::mutex _filemtx;
std::map<std::string, std::string> loadfile(std::atomic<bool>& stopFlag, const std::string& filename)
{

    std::ifstream  i_n(filename);

    std::map<std::string, std::string>  kv;
    if (!i_n) {

        std::cerr << "error openen  file sream " << std::endl;
        i_n.close();
    }
    std::string line;

    while (std::getline(i_n, line) && !stopFlag.load()) {
        {
            std::lock_guard <std::mutex> lk_(_filemtx);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::cout << " file reader  " << std::endl;
        auto pos = line.find("secret");
        std::string tag = "secret is";
        size_t n_ = std::string("[PYTHON]").length();
        std::cout << pos << " n_" << n_ << std::endl;
        if (line.find("secret") != std::string::npos) {
            std::cout << line << std::endl;
            std::cout << "secret find  " << std::endl;
            std::string  k = line.substr(n_, pos + 1);
            std::string v = line.substr(tag.length() + 2);
            std::cout << "key is   " << k << " value  " << v;
            if (!k.empty() && !v.empty()) kv[k] = v;
        }
        //std::cout << line << std::endl; 


    }
    //if(!kv.empty())
    //for (auto &mp : kv) std::cout << mp.first << mp.second << std::endl;
    i_n.close();
    return kv;
}


// Forward declarations
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Simple helper to convert TaskState -> color + text
struct VisualInfo {
    COLORREF color;
    std::wstring text;
};
VisualInfo state_visual(TaskState s) {
    switch (s) {
    case TaskState::Ready:   return { RGB(200,200,0), L"Ready" };   // yellow-ish
    case TaskState::Running: return { RGB(255,220,0), L"Running" }; // yellow
    case TaskState::Finished:return { RGB(0,200,0), L"Finished" }; // green
    case TaskState::Stopped: return { RGB(200,0,0), L"Stopped" };  // red
    case TaskState::Error:   return { RGB(150,0,0), L"Error" };    // dark red
    default: return { RGB(150,150,150), L"Unknown" };
    }
}

// Example worker functions
// 1) long running loop that checks stop flag
void example_worker(std::atomic<bool>& stopFlag) {
    // simulate work in steps and notify UI via PostMessage
    for (int i = 0; i < 10 && !stopFlag.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        // inform UI: we will post WM_TASK_UPDATE to the window
        if (g_hwnd) PostMessage(g_hwnd, WM_TASK_UPDATE, 0, 0);
    }
}

// 2) async pattern that itself creates a thread inside
void nested_async_worker(std::atomic<bool>& stopFlag) {
    // Start nested thread that simulates some subtask
    std::thread nested([](std::atomic<bool>& sf) {
        for (int j = 0; j < 5 && !sf.load(); ++j) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            if (g_hwnd) PostMessage(g_hwnd, WM_TASK_UPDATE, 0, 0);
        }
        }, std::ref(stopFlag));

    // Meanwhile do this task's main loop
    for (int i = 0; i < 6 && !stopFlag.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(350));
        if (g_hwnd) PostMessage(g_hwnd, WM_TASK_UPDATE, 0, 0);
    }

    // Join nested
    if (nested.joinable()) nested.join();
}

// UI: paint current tasks
void paint_tasks(HDC hdc) {
    if (!g_taskManager) return;
    auto snap = g_taskManager->snapshot();
    int y = 10;
    int h = 24;
    for (auto& p : snap) {
        std::wstring name;
        {
            // convert name to wide (assuming ASCII/UTF-8 safe short names)
            std::string n = p.first;
            name = std::wstring(n.begin(), n.end());
        }
        VisualInfo vi = state_visual(p.second);

        // draw rectangle background
        RECT r = { 10, y, 500, y + h };
        HBRUSH br = CreateSolidBrush(vi.color);
        FillRect(hdc, &r, br);
        DeleteObject(br);

        // draw text: "TaskName - StateText"
        std::wostringstream oss;
        oss << name << L" - " << vi.text;
        SetBkMode(hdc, TRANSPARENT);
        DrawTextW(hdc, oss.str().c_str(), -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        y += h + 6;
    }
}

void run_server(std::atomic<bool>& stop, unsigned short  port = 9002)
{

    WSADATA wSdata;
    int _ret = WSAStartup(MAKEWORD(2, 2), &wSdata);
    if (_ret != 0) {
        std::cout << "wstartup filed" << std::endl; 
        return;
    }

    SOCKET socket_server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_server == INVALID_SOCKET) {
        std::cerr << "errro  scoket() failed" << std::endl;
        WSACleanup();
        return;

    }

    sockaddr_in socker_in{};
    socker_in.sin_addr.s_addr = INADDR_ANY; 
    socker_in.sin_family = AF_INET;
    socker_in.sin_port = htons(port);


    int opt;

    setsockopt(socket_server, SOL_SOCKET, opt, (char*)&opt, sizeof(opt));


    if (bind(socket_server, (sockaddr*)&socker_in, sizeof(socker_in)) == SOCKET_ERROR) {
        std::cerr << "bind failed\n";
        closesocket(socket_server);
        WSACleanup();
        return;
    }

    if (listen(socket_server, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen failed\n";
        closesocket(socket_server);
        WSACleanup();
        return;
    }

    std::cout << "Server listening on port " << port << std::endl;


}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Multithread Tasks Status",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 540, 420,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd) return 0;
    g_hwnd = hwnd;

    // Create task manager with hwnd so tasks can PostMessage to it.
    g_taskManager = std::make_unique<TaskManager>(g_hwnd);

    // Create tasks with names. Some names include "async" to trigger std::async pattern
    auto t1 = std::make_shared<Task>("Task-1", [](std::atomic<bool>& stop) { example_worker(stop); });
    auto t2 = std::make_shared<Task>("Task-2-async", [](std::atomic<bool>& stop) { nested_async_worker(stop); });
    auto t4 = std::make_shared <Task>("python file", [path = "python app.py"](std::atomic<bool>& stop) {
        python_task_runner(stop, path);
        });

    auto t8 = std::make_shared<Task>("load loadfile ", [](std::atomic<bool>& stop) {
        loadfile(stop, "python_data.log");
        });
    std::string b = "ayoub";
    std::string a = "10";
    std::string  c = "2025";
    int d = 20250;
    std::stringstream  _path;
    //std::sprintf ((char *)_path.c_str(), "python sha256.py %s %s %s %d", a, b.c_str(), c.c_str(), d); 
    _path << "python sha256.py " << a << " " << b << " " << c << " " << d;
    std::cout << "lancer  " << _path.str() << std::endl;
    auto t6 = std::make_shared <Task>("python file", [path = _path.str()](std::atomic<bool>& stop) {
        python_task_runner(stop, path);
        });
    auto t7 = std::make_shared <Task>("python file", [path = "python blockchain.py"](std::atomic<bool>& stop) {
        python_task_runner(stop, path);
        });

    auto t3 = std::make_shared<Task>("Task-3", [](std::atomic<bool>& stop) {

        // simulate different durations and occasional early stop
        for (int i = 0; i < 8 && !stop.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            if (g_hwnd) PostMessage(g_hwnd, WM_TASK_UPDATE, 0, 0);
        }
        });
    std::vector<int> vc{ 1,2,3,5,6,9,10 };
    auto t54 = std::make_shared<Task>("atomic share", [vc](std::atomic<bool>& stop) {
        data_vect_create(vc, 10, 0);
        });
    std::string filename = "python_data.log";
    std::map<std::string, std::string> kv;

    auto t89 = std::make_shared<Task>("load file ok", [filename](std::atomic<bool>& stop) {
        std::future<std::map<std::string, std::string>> res = std::async(std::launch::async, loadfile, std::ref(stop), filename);
        res.get();
        });
    auto t10 = std::make_shared<Task>("stpe", [path = "python step.py"](std::atomic<bool>& stop) {
        python_task_runner(stop, path);
        });
    auto t1014 = std::make_shared<Task>("mainUDP", [path = "mainUDP"](std::atomic<bool>& stop) {
        mainUDP();
        });
    auto t1014ex = std::make_shared<Task>("appfinal", [path = "appfinal.exe"](std::atomic<bool>& stop) {
        python_task_runner(stop, path);
        });

    auto t1014ex_appmo = std::make_shared<Task>("_appmo", [path = "_appmo.exe"](std::atomic<bool>& stop) {
        _appmo();
        });
    g_taskManager->add_task(t1014ex);
    g_taskManager->add_task(t1014ex_appmo);
    g_taskManager->add_task(t1);
    g_taskManager->add_task(t2);
    g_taskManager->add_task(t3);
    g_taskManager->add_task(t4);
    g_taskManager->add_task(t54);
    g_taskManager->add_task(t6);
    g_taskManager->add_task(t7);
    g_taskManager->add_task(t89);
    g_taskManager->add_task(t10);
    g_taskManager->add_task(t1014);

    ShowWindow(hwnd, nCmdShow);

    // simple UI controls: Start button + Stop button using system timers or simple keys
    // We'll start tasks automatically after a brief moment using a timer
    SetTimer(hwnd, 1, 500, nullptr);

    // Standard message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // cleanup
    g_taskManager.reset();
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        return 0;

    case WM_TIMER:
        if (wParam == 1) {
            // Start tasks on first timer tick and kill timer
            KillTimer(hwnd, 1);
            if (g_taskManager) {
                g_taskManager->start_all();
            }
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        paint_tasks(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_TASK_UPDATE:
        // UI got a notification that some task changed
        InvalidateRect(hwnd, nullptr, FALSE); // request repaint
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
        }
        else if (wParam == 'S') {
            // Stop all
            if (g_taskManager) {
                g_taskManager->stop_all();
            }
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        else if (wParam == 'R') {
            // Restart: (for demo - app exit and relaunch of tasks not implemented)
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}



int main() {

    HINSTANCE Hinstance = GetModuleHandle(nullptr);
    

    return wWinMain(Hinstance, nullptr, (PWSTR)GetCommandLineA(), SW_SHOW);
}   