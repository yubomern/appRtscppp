cmake_minimum_required(VERSION 3.16)
project(RTSPMultiChannelServer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── GStreamer Root (Windows MSVC) ───────────────────────────────────────────
# Change this path to match your GStreamer installation
set(GST_ROOT "C:/Program Files/gstreamer/1.0/msvc_x86_64"
    CACHE PATH "GStreamer installation root")

# ─── Include directories ─────────────────────────────────────────────────────
include_directories(
    "${GST_ROOT}/include/gstreamer-1.0"
    "${GST_ROOT}/include/glib-2.0"
    "${GST_ROOT}/lib/glib-2.0/include"
    "${GST_ROOT}/include/gstreamer-rtsp-server-1.0"
)

# ─── Library directories ─────────────────────────────────────────────────────
link_directories("${GST_ROOT}/lib")

# ─── Executable ──────────────────────────────────────────────────────────────
add_executable(rtsp_server src/rtsp_server.cpp)

# ─── Link GStreamer libraries ─────────────────────────────────────────────────
target_link_libraries(rtsp_server
    gstreamer-1.0
    gobject-2.0
    glib-2.0
    gstbase-1.0
    gstrtspserver-1.0   # RTSP server library
    gstvideo-1.0        # Video elements
)

# ─── Windows-specific settings ───────────────────────────────────────────────
if(WIN32)
    target_compile_definitions(rtsp_server PRIVATE
        _WIN32_WINNT=0x0601
        NOMINMAX
    )
    # Copy config folder next to executable
    add_custom_command(TARGET rtsp_server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/config"
            "$<TARGET_FILE_DIR:rtsp_server>/config"
        COMMENT "Copying config folder..."
    )
endif()

# ─── Linux/macOS pkg-config path ─────────────────────────────────────────────
if(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GST REQUIRED
        gstreamer-1.0
        gstreamer-rtsp-server-1.0
        glib-2.0
        gobject-2.0
    )
    target_include_directories(rtsp_server PRIVATE ${GST_INCLUDE_DIRS})
    target_link_libraries(rtsp_server ${GST_LIBRARIES})
    target_compile_options(rtsp_server PRIVATE ${GST_CFLAGS_OTHER})
endif()

# ─── Install ─────────────────────────────────────────────────────────────────
install(TARGETS rtsp_server DESTINATION bin)
install(DIRECTORY config DESTINATION share/rtsp_server)

message(STATUS "Build configured: RTSP Multi-Channel Server")
message(STATUS "GST_ROOT = ${GST_ROOT}")
