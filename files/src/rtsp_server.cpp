/**
 * RTSP Multi-Channel Server with Config Hot-Reload & Text Overlay
 * ================================================================
 * Author  : RTSP Project
 * Version : 1.0.0
 *
 * Features:
 *   - Multi-channel RTSP streaming via GStreamer RTSP Server
 *   - JSON config hot-reload (no restart needed)
 *   - Per-channel text overlay with timestamp
 *   - Dynamic add/remove channels
 *   - VLC-compatible H.264/RTP streams
 *
 * Build:
 *   cmake -B build && cmake --build build
 *
 * Config file: config/rtsp_config.json
 * Stream URL example: rtsp://localhost:8554/stream1
 */

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <glib.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include <ctime>
#include <sys/stat.h>

/* ─── Simple JSON value extraction helpers ─── */
static std::string json_str(const std::string& json, const std::string& key) {
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";
    pos = json.find("\"", pos);
    if (pos == std::string::npos) return "";
    auto end = json.find("\"", pos + 1);
    return json.substr(pos + 1, end - pos - 1);
}

static int json_int(const std::string& json, const std::string& key, int def = 0) {
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return def;
    pos = json.find(":", pos);
    if (pos == std::string::npos) return def;
    while (pos < json.size() && !isdigit(json[pos])) ++pos;
    if (pos >= json.size()) return def;
    return std::stoi(json.substr(pos));
}

static bool json_bool(const std::string& json, const std::string& key, bool def = true) {
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return def;
    pos = json.find(":", pos);
    if (pos == std::string::npos) return def;
    pos = json.find_first_not_of(" \t\r\n", pos + 1);
    return json.substr(pos, 4) != "fals";
}

/* ─── Channel configuration ─── */
struct ChannelConfig {
    int         id{0};
    std::string name;
    std::string mount;
    std::string source{"videotestsrc"};
    int         pattern{0};
    std::string text_overlay;
    bool        enabled{true};
};

/* ─── Parse all channels from JSON string ─── */
static std::vector<ChannelConfig> parse_channels(const std::string& json) {
    std::vector<ChannelConfig> channels;
    size_t start = json.find("\"channels\"");
    if (start == std::string::npos) return channels;

    // Find the array [ ... ]
    start = json.find("[", start);
    int depth = 0;
    size_t i = start;
    for (; i < json.size(); ++i) {
        if (json[i] == '[' || json[i] == '{') ++depth;
        if (json[i] == ']' || json[i] == '}') {
            --depth;
            if (depth == 0) { ++i; break; }
        }
    }
    std::string arr = json.substr(start, i - start);

    // Split objects
    size_t obj_start = 0;
    int d2 = 0;
    for (size_t j = 0; j < arr.size(); ++j) {
        if (arr[j] == '{') { if (d2 == 0) obj_start = j; ++d2; }
        if (arr[j] == '}') {
            --d2;
            if (d2 == 0) {
                std::string obj = arr.substr(obj_start, j - obj_start + 1);
                ChannelConfig ch;
                ch.id           = json_int(obj, "id");
                ch.name         = json_str(obj, "name");
                ch.mount        = json_str(obj, "mount");
                ch.source       = json_str(obj, "source");
                ch.pattern      = json_int(obj, "pattern");
                ch.text_overlay = json_str(obj, "text_overlay");
                ch.enabled      = json_bool(obj, "enabled");
                if (!ch.mount.empty()) channels.push_back(ch);
            }
        }
    }
    return channels;
}

/* ─── Build GStreamer launch string for a channel ─── */
static std::string build_pipeline(const ChannelConfig& ch) {
    std::ostringstream oss;
    if (ch.source == "videotestsrc") {
        oss << "( videotestsrc pattern=" << ch.pattern << " is-live=true "
            << "! video/x-raw,width=640,height=480,framerate=25/1 "
            << "! clockoverlay time-format=\"%H:%M:%S\" font-desc=\"Sans 14\" "
               "  valignment=bottom halignment=right "
            << "! textoverlay text=\"" << ch.text_overlay << "\" "
               "  valignment=top halignment=left font-desc=\"Sans Bold 16\" "
            << "! videoconvert "
            << "! x264enc tune=zerolatency bitrate=800 speed-preset=superfast "
            << "! rtph264pay name=pay0 pt=96 )";
    } else {
        // File source fallback
        oss << "( filesrc location=" << ch.source << " "
            << "! decodebin "
            << "! videoconvert "
            << "! textoverlay text=\"" << ch.text_overlay << "\" "
               "  valignment=top halignment=left font-desc=\"Sans Bold 16\" "
            << "! x264enc tune=zerolatency bitrate=800 "
            << "! rtph264pay name=pay0 pt=96 )";
    }
    return oss.str();
}

/* ─── RTSP Server Manager ─── */
class RTSPServerManager {
public:
    GstRTSPServer*   server_{nullptr};
    GstRTSPMountPoints* mounts_{nullptr};
    GMainLoop*       loop_{nullptr};
    std::mutex       mtx_;
    std::map<std::string, GstRTSPMediaFactory*> factories_;

    int    port_{8554};
    std::string host_{"0.0.0.0"};

    void init(int port, const std::string& host) {
        port_ = port;
        host_ = host;
        server_ = gst_rtsp_server_new();
        gst_rtsp_server_set_address(server_, host.c_str());
        gst_rtsp_server_set_service(server_, std::to_string(port).c_str());
        mounts_ = gst_rtsp_server_get_mount_points(server_);
        loop_ = g_main_loop_new(nullptr, FALSE);
    }

    void add_channel(const ChannelConfig& ch) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (factories_.count(ch.mount)) {
            g_print("[INFO] Mount %s already exists, skipping.\n", ch.mount.c_str());
            return;
        }
        auto* factory = gst_rtsp_media_factory_new();
        std::string pipeline = build_pipeline(ch);
        g_print("[INFO] Adding channel: %s -> %s\n", ch.mount.c_str(), pipeline.c_str());
        gst_rtsp_media_factory_set_launch(factory, pipeline.c_str());
        gst_rtsp_media_factory_set_shared(factory, TRUE);
        gst_rtsp_mount_points_add_factory(mounts_, ch.mount.c_str(), factory);
        factories_[ch.mount] = factory;
        g_print("[OK]  Stream ready: rtsp://%s:%d%s\n", host_.c_str(), port_, ch.mount.c_str());
    }

    void remove_channel(const std::string& mount) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!factories_.count(mount)) return;
        gst_rtsp_mount_points_remove_factory(mounts_, mount.c_str());
        factories_.erase(mount);
        g_print("[INFO] Removed channel: %s\n", mount.c_str());
    }

    void load_channels(const std::vector<ChannelConfig>& channels) {
        for (auto& ch : channels) {
            if (ch.enabled) add_channel(ch);
        }
    }

    void start() {
        gst_rtsp_server_attach(server_, nullptr);
        g_print("[SERVER] RTSP server running on rtsp://%s:%d\n", host_.c_str(), port_);
        g_main_loop_run(loop_);
    }

    void stop() {
        if (loop_) g_main_loop_quit(loop_);
    }
};

/* ─── Config File Watcher ─── */
class ConfigWatcher {
public:
    std::string         path_;
    time_t              last_mtime_{0};
    std::atomic<bool>   running_{false};
    std::thread         watcher_thread_;
    std::function<void(const std::string&)> on_change_;

    void start(const std::string& path,
               std::function<void(const std::string&)> cb) {
        path_      = path;
        on_change_ = cb;
        running_   = true;
        last_mtime_ = get_mtime();
        watcher_thread_ = std::thread([this]() {
            while (running_) {
                std::this_thread::sleep_for(std::chrono::seconds(2));
                time_t mt = get_mtime();
                if (mt != last_mtime_) {
                    last_mtime_ = mt;
                    g_print("[WATCHER] Config changed, reloading...\n");
                    std::ifstream f(path_);
                    if (f) {
                        std::string content((std::istreambuf_iterator<char>(f)),
                                             std::istreambuf_iterator<char>());
                        on_change_(content);
                    }
                }
            }
        });
    }

    void stop() {
        running_ = false;
        if (watcher_thread_.joinable()) watcher_thread_.join();
    }

private:
    time_t get_mtime() {
        struct stat st;
        if (stat(path_.c_str(), &st) == 0) return st.st_mtime;
        return 0;
    }
};

/* ─── Main ─── */
int main(int argc, char** argv) {
    gst_init(&argc, &argv);

    std::string config_path = "config/rtsp_config.json";
    if (argc > 1) config_path = argv[1];

    // Load initial config
    std::ifstream f(config_path);
    if (!f) {
        std::cerr << "[ERROR] Cannot open config: " << config_path << "\n";
        return 1;
    }
    std::string json((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());

    int port = json_int(json, "port", 8554);
    std::string host = json_str(json, "host");
    if (host.empty()) host = "0.0.0.0";

    auto channels = parse_channels(json);
    g_print("[CONFIG] Loaded %zu channel(s) from %s\n",
            channels.size(), config_path.c_str());

    // Build server
    RTSPServerManager mgr;
    mgr.init(port, host);
    mgr.load_channels(channels);

    // Config hot-reload watcher
    ConfigWatcher watcher;
    watcher.start(config_path, [&](const std::string& new_json) {
        // Simple reload: remove old mounts, add new ones
        int new_port = json_int(new_json, "port", 8554);
        auto new_channels = parse_channels(new_json);

        // Collect new mounts
        std::map<std::string, ChannelConfig> new_map;
        for (auto& c : new_channels) new_map[c.mount] = c;

        // Remove disappeared mounts
        std::vector<std::string> to_remove;
        {
            std::lock_guard<std::mutex> lock(mgr.mtx_);
            for (auto& kv : mgr.factories_) {
                if (!new_map.count(kv.first)) to_remove.push_back(kv.first);
            }
        }
        for (auto& m : to_remove) mgr.remove_channel(m);

        // Add new / skip existing
        for (auto& ch : new_channels) {
            if (ch.enabled) mgr.add_channel(ch);
            else            mgr.remove_channel(ch.mount);
        }
        g_print("[RELOAD] Config reload complete.\n");
    });

    // Print usage info
    g_print("\n========================================\n");
    g_print("  RTSP Multi-Channel Server v1.0\n");
    g_print("========================================\n");
    for (auto& ch : channels) {
        if (ch.enabled)
            g_print("  [%d] %s  =>  rtsp://%s:%d%s\n",
                    ch.id, ch.name.c_str(),
                    host.c_str(), port, ch.mount.c_str());
    }
    g_print("  Config auto-reload: ON (%s)\n", config_path.c_str());
    g_print("  Open in VLC: Media > Open Network Stream\n");
    g_print("========================================\n\n");

    // Run GLib main loop (blocking)
    mgr.start();

    watcher.stop();
    return 0;
}
