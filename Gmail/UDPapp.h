#pragma once
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <sstream>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <map>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib,"ws2_32.lib")
using socklen_t = int;
#define CLOSE_SOCK closesocket
#define SOCK_ERR   WSAEWOULDBLOCK
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1
#define CLOSE_SOCK     close
#define SOCKET         int
#endif

 // ─────────────────────────────────────────────────────────────
 // CONFIG
 // ─────────────────────────────────────────────────────────────
constexpr int TCP_PORT = 5000;
constexpr int UDP_PORT = 5001;
constexpr int BACKLOG = 8;
constexpr int BUF_SIZE = 4096;

std::mutex          g_log_mtx;
std::atomic<bool>   g_running{ true };

// ─────────────────────────────────────────────────────────────
// LOGGING
// ─────────────────────────────────────────────────────────────
void log(const std::string& tag, const std::string& msg) {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::lock_guard<std::mutex> lk(g_log_mtx);
 
    std::cout << "["  << "]"
        << "[" << tag << "] " << msg << "\n";
}

// ─────────────────────────────────────────────────────────────
// COMMAND PROCESSOR (shared by TCP & UDP)
// ─────────────────────────────────────────────────────────────
std::string process_command(const std::string& raw) {
    // Trim
    std::string cmd = raw;
    while (!cmd.empty() && (cmd.back() == '\r' || cmd.back() == '\n' || cmd.back() == ' '))
        cmd.pop_back();

    log("CMD", "recv: " + cmd);

    // ── Command table ────────────────────────────────────────
    if (cmd == "PING")              return "PONG\r\n";
    if (cmd == "FLASH_Read ALL")    return "FLASH_DATA: [OK] 4MB READ COMPLETE\r\n";
    if (cmd == "eth_init")          return "OK eth interface initialized\r\n";
    if (cmd == "ifconfig")          return "eth0: flags=4163<UP,BROADCAST,RUNNING,MULTICAST> mtu 1500\r\n"
        "        inet 192.168.1.100  netmask 255.255.255.0  broadcast 192.168.1.255\r\n";
    if (cmd == "ifconfig eth0 up")  return "OK\r\n";
    if (cmd.rfind("udhcpc", 0) == 0)   return "udhcpc: sending discover\r\nIP obtained: 192.168.1.100\r\n";
    if (cmd.rfind("ping ", 0) == 0) {
        std::string ip = cmd.substr(5);
        return "PING " + ip + ": 56 data bytes\r\n"
            "64 bytes from " + ip + ": seq=1 ttl=64 time=0.423 ms\r\n"
            "64 bytes from " + ip + ": seq=2 ttl=64 time=0.401 ms\r\n"
            "--- ping statistics ---\r\n"
            "2 packets transmitted, 2 received, 0% packet loss\r\n";
    }
    if (cmd == "STATUS")  return "SYSTEM: OK | UPTIME: running | SLOTS: 8\r\n";
    if (cmd == "VERSION") return "FW_VER: 2.0.0 | HW_REV: B\r\n";
    if (cmd == "RESET")   return "RESETTING...\r\nOK\r\n";

    return "ERR: Unknown command: " + cmd + "\r\n";
}

// ─────────────────────────────────────────────────────────────
// TCP CLIENT HANDLER
// ─────────────────────────────────────────────────────────────
void tcp_client_handler(SOCKET client_sock, std::string peer_addr) {
    log("TCP", "Client connected: " + peer_addr);

    // Send banner / tt> prompt
    std::string banner = "\r\n=== Production Test Server v2.0 ===\r\ntt> ";
    send(client_sock, banner.c_str(), (int)banner.size(), 0);

    char buf[BUF_SIZE];
    std::string line_buf;

    while (g_running) {
        int n = recv(client_sock, buf, BUF_SIZE - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        line_buf += buf;

        // Process complete lines
        size_t pos;
        while ((pos = line_buf.find('\n')) != std::string::npos) {
            std::string line = line_buf.substr(0, pos);
            line_buf.erase(0, pos + 1);

            std::string resp = process_command(line);
            resp += "tt> ";  // re-issue prompt
            send(client_sock, resp.c_str(), (int)resp.size(), 0);
        }
    }

    CLOSE_SOCK(client_sock);
    log("TCP", "Client disconnected: " + peer_addr);
}

// ─────────────────────────────────────────────────────────────
// TCP SERVER THREAD
// ─────────────────────────────────────────────────────────────
void tcp_server_thread() {
    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == INVALID_SOCKET) { log("TCP", "socket() failed"); return; }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(TCP_PORT);

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        log("TCP", "bind() failed"); CLOSE_SOCK(srv); return;
    }
    listen(srv, BACKLOG);
    log("TCP", "Listening on port " + std::to_string(TCP_PORT));

    std::vector<std::thread> clients;
    while (g_running) {
        sockaddr_in cli_addr{};
        socklen_t cli_len = sizeof(cli_addr);
        SOCKET cli = accept(srv, (sockaddr*)&cli_addr, &cli_len);
        if (cli == INVALID_SOCKET) continue;

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli_addr.sin_addr, ip_str, sizeof(ip_str));
        std::string peer = std::string(ip_str) + ":" + std::to_string(ntohs(cli_addr.sin_port));

        clients.emplace_back(tcp_client_handler, cli, peer);
        clients.back().detach();
    }
    CLOSE_SOCK(srv);
}

// ─────────────────────────────────────────────────────────────
// UDP SERVER THREAD
// ─────────────────────────────────────────────────────────────
void udp_server_thread() {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) { log("UDP", "socket() failed"); return; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(UDP_PORT);

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        log("UDP", "bind() failed"); CLOSE_SOCK(sock); return;
    }
    log("UDP", "Listening on port " + std::to_string(UDP_PORT));

    char buf[BUF_SIZE];
    while (g_running) {
        sockaddr_in cli_addr{};
        socklen_t   cli_len = sizeof(cli_addr);
        int n = recvfrom(sock, buf, BUF_SIZE - 1, 0, (sockaddr*)&cli_addr, &cli_len);
        if (n <= 0) continue;
        buf[n] = '\0';

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli_addr.sin_addr, ip_str, sizeof(ip_str));
        log("UDP", std::string("from ") + ip_str + ": " + buf);

        std::string resp = process_command(std::string(buf, n));
        sendto(sock, resp.c_str(), (int)resp.size(), 0,
            (sockaddr*)&cli_addr, cli_len);
    }
    CLOSE_SOCK(sock);
}

// ─────────────────────────────────────────────────────────────
// MAIN
// ─────────────────────────────────────────────────────────────
int mainUDP() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n"; return 1;
    }
#endif

    std::cout << R"(
  ____  ____  ____  ____   __    __     ____  ____  ___  ____
 / ___)(  __)(  _ \(  _ \ / _\  (  )   (_  _)(  __)/ __)(_  _)
 \___ \ ) _)  )   / ) __//    \  )(__   )(    ) _))\__ \  )(
 (____/(____)(____)(__)  \_/\_/(____/  (__) (____)(___/ (__)

 TCP Port: )" << TCP_PORT << "   UDP Port: " << UDP_PORT << "\n\n";

    std::thread t1(tcp_server_thread);
    std::thread t2(udp_server_thread);

    std::cout << "Press ENTER to quit...\n";
    std::cin.get();
    g_running = false;

    t1.join();
    t2.join();

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
