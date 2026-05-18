cmake_minimum_required(VERSION 3.20)
project(VirtualBench VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tell CMake where to find Qt6
set(CMAKE_PREFIX_PATH "C:/Qt/6.11.1/mingw_64")

# Qt6 modules we need right now
# We'll add WebEngineWidgets later for the Monaco editor
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Auto-run Qt's code generators on files that need it
# MOC  = processes Q_OBJECT macros
# UIC  = processes .ui files (we don't have any yet)
# RCC  = processes .qrc resource files (we don't have any yet)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# All source files for the main application
set(SOURCES
    src/main.cpp
    src/ui/mainwindow.cpp
    src/ui/canvaswidget.cpp
    src/core/runtime/arduinoruntime.cpp
    src/core/host/sketchhost.cpp
    src/core/host/sketchhostthread.cpp
    src/core/build/compiler.cpp
    src/core/circuit/circuitdetector.cpp
)

set(HEADERS
    src/ui/mainwindow.h
    src/ui/canvaswidget.h
    src/core/runtime/arduinoapi.h
    src/core/runtime/arduinoruntime.h
    src/core/host/sketchhost.h
    src/core/host/sketchhostthread.h
    src/core/build/compiler.h
    src/core/circuit/circuitdetector.h
)

# Create the executable
qt_add_executable(VirtualBench ${SOURCES} ${HEADERS})

# Include paths -- everything resolves from the project root
target_include_directories(VirtualBench PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core/runtime
    ${CMAKE_SOURCE_DIR}/src/core/host
    ${CMAKE_SOURCE_DIR}/src/ui
)

# Link Qt modules
target_link_libraries(VirtualBench PRIVATE
    Qt6::Core
    Qt6::Widgets
)

# Windows-specific: hide the console window in release builds
# In debug builds we keep it so we can see std::cout output
if(WIN32)
    set_target_properties(VirtualBench PROPERTIES
        WIN32_EXECUTABLE $<CONFIG:Release>
    )
endif()