cmake_minimum_required(VERSION 3.16)
project(GStreamerAudioTutorial)

set(CMAKE_CXX_STANDARD 17)

# Specify the GStreamer root path (change if needed)
set(GST_ROOT "C:/Program Files/gstreamer/1.0/msvc_x86_64")

# Include GStreamer headers
include_directories(
    "${GST_ROOT}/include/gstreamer-1.0"
    "${GST_ROOT}/include/glib-2.0"
    "${GST_ROOT}/lib/glib-2.0/include"
    "${GST_ROOT}/include/gstreamer-rtsp-server-1.0"  # Needed for RTSP server headers if you use them
)

# Link against GStreamer libraries
link_directories("${GST_ROOT}/lib")

# Create the executable
add_executable(code_rtsp code_rtsp.cpp)

# Link the GStreamer libraries we need
target_link_libraries(code_rtsp
    gstreamer-1.0
    gobject-2.0
    glib-2.0
    gstbase-1.0
    gstrtspserver-1.0  # Only if you are using RTSP server; omit if unnecessary
)