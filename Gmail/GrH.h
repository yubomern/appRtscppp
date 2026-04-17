#define UNICODE
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <string>

// --------------------------------------------------
// Globals
// --------------------------------------------------
HWND g_hLog = nullptr;

// --------------------------------------------------
// Append text to EDIT control
// --------------------------------------------------
void AppendText(HWND hwnd, const std::wstring& text)
{
    int len = GetWindowTextLengthW(hwnd);
    SendMessageW(hwnd, EM_SETSEL, len, len);
    SendMessageW(hwnd, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
}

// --------------------------------------------------
// Run a command using CreateProcessW
// --------------------------------------------------
void RunCommand(HWND log, const std::wstring& command)
{
    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    std::wstring cmdLine = command;

    if (CreateProcessW(
        nullptr,
       (LPWSTR) cmdLine.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi))
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        AppendText(log, L"OK\n");
    }
    else
    {
        AppendText(log, L"ERROR running command\n");
    }
}

// --------------------------------------------------
// Enable Ethernet
// --------------------------------------------------
void EnableEthernet(HWND log)
{
    AppendText(log, L"Enabling Ethernet...\r\n");
    RunCommand(
        log,
        L"netsh interface set interface \"Ethernet\" admin=ENABLED"
    );
}

// --------------------------------------------------
// Renew DHCP
// --------------------------------------------------
void RenewDhcp(HWND log)
{
    AppendText(log, L"Renewing DHCP...\r\n");
    RunCommand(log, L"ipconfig /renew");
}

// --------------------------------------------------
// Window procedure
// --------------------------------------------------
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_COMMAND:
        if (LOWORD(wParam) == 1)
            EnableEthernet(g_hLog);
        else if (LOWORD(wParam) == 2)
            RenewDhcp(g_hLog);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// --------------------------------------------------
// WinMain
// --------------------------------------------------
int WINAPI wWinMainV(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"EthernetWinAPI";

    WNDCLASS wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Ethernet Control (No Boost)",
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        520, 350,
        nullptr, nullptr, hInst, nullptr);

    CreateWindow(L"BUTTON", L"Enable Ethernet",
        WS_VISIBLE | WS_CHILD,
        20, 20, 200, 35,
        hwnd, (HMENU)1, hInst, nullptr);

    CreateWindow(L"BUTTON", L"Renew DHCP",
        WS_VISIBLE | WS_CHILD,
        20, 65, 200, 35,
        hwnd, (HMENU)2, hInst, nullptr);

    g_hLog = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_VISIBLE | WS_CHILD | ES_MULTILINE |
        WS_VSCROLL | ES_AUTOVSCROLL | ES_READONLY,
        20, 120, 460, 180,
        hwnd, nullptr, hInst, nullptr);

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

int _appmo() {
    HINSTANCE hInstance=GetModuleHandle(nullptr);
    return wWinMainV(hInstance, nullptr, GetCommandLine(), SHOW_FULLSCREEN);
}