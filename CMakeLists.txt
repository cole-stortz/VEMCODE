cmake_minimum_required(VERSION 3.20)
project(VEMCODE VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate compile_commands.json for IDE language servers (clangd, IntelliSense)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Prefer native system Qt6 over cross-compiled versions
list(PREPEND CMAKE_PREFIX_PATH /usr/lib64)

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

# Embed injected_header.h as a C string at configure time.
# set_property tells the build system to re-run cmake when the file changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/core/build/injected_header.inc")
file(READ "${CMAKE_SOURCE_DIR}/src/core/build/injected_header.inc" INJECTED_HEADER_CONTENT)
configure_file(
    "${CMAKE_SOURCE_DIR}/src/core/build/injected_header.cpp.in"
    "${CMAKE_BINARY_DIR}/generated/injected_header.cpp"
    @ONLY
)

# Embed injectable library .inc files as C strings at configure time.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/core/build/libs/servo.inc"
    "${CMAKE_SOURCE_DIR}/src/core/build/libs/liquidcrystal.inc"
    "${CMAKE_SOURCE_DIR}/src/core/build/libs/softwareserial.inc")

file(READ "${CMAKE_SOURCE_DIR}/src/core/build/libs/servo.inc" SERVO_LIB_CONTENT)
configure_file(
    "${CMAKE_SOURCE_DIR}/src/core/build/libs/servo_lib.cpp.in"
    "${CMAKE_BINARY_DIR}/generated/servo_lib.cpp"
    @ONLY
)

file(READ "${CMAKE_SOURCE_DIR}/src/core/build/libs/liquidcrystal.inc" LIQUIDCRYSTAL_LIB_CONTENT)
configure_file(
    "${CMAKE_SOURCE_DIR}/src/core/build/libs/liquidcrystal_lib.cpp.in"
    "${CMAKE_BINARY_DIR}/generated/liquidcrystal_lib.cpp"
    @ONLY
)

file(READ "${CMAKE_SOURCE_DIR}/src/core/build/libs/softwareserial.inc" SOFTWARESERIAL_LIB_CONTENT)
configure_file(
    "${CMAKE_SOURCE_DIR}/src/core/build/libs/softwareserial_lib.cpp.in"
    "${CMAKE_BINARY_DIR}/generated/softwareserial_lib.cpp"
    @ONLY
)

# Component plugin files — drop a .cpp into src/components/ and it's picked up automatically
file(GLOB COMPONENT_SOURCES "${CMAKE_SOURCE_DIR}/src/components/*.cpp")

# All source files for the main application
set(SOURCES
    src/main.cpp
    src/ui/mainwindow.cpp
    src/ui/canvaswidget.cpp
    src/ui/signaltimeline.cpp
    src/ui/codehighlighter.cpp
    src/ui/linenumberarea.cpp
    src/ui/variablewatch.cpp
    src/ui/devicespanel.cpp
    src/ui/settingsdialog.cpp
    src/core/runtime/arduinoruntime.cpp
    src/core/host/sketchhost.cpp
    src/core/host/sketchhostthread.cpp
    src/core/build/compiler.cpp
    src/core/build/preprocessor.cpp
    src/core/circuit/circuitdetector.cpp
    src/core/circuit/componentitem.cpp
    src/core/circuit/componentregistry.cpp
    src/lsan_suppressions.cpp
    ${CMAKE_BINARY_DIR}/generated/injected_header.cpp
    ${CMAKE_BINARY_DIR}/generated/servo_lib.cpp
    ${CMAKE_BINARY_DIR}/generated/liquidcrystal_lib.cpp
    ${CMAKE_BINARY_DIR}/generated/softwareserial_lib.cpp
    ${COMPONENT_SOURCES}
)

set(HEADERS
    src/ui/mainwindow.h
    src/ui/canvaswidget.h
    src/ui/signaltimeline.h
    src/ui/codehighlighter.h
    src/ui/linenumberarea.h
    src/ui/variablewatch.h
    src/ui/devicespanel.h
    src/ui/settingsdialog.h
    src/core/runtime/arduinoapi.h
    src/core/runtime/arduinoruntime.h
    src/core/host/sketchhost.h
    src/core/host/sketchhostthread.h
    src/core/build/compiler.h
    src/core/build/preprocessor.h
    src/core/circuit/circuitdetector.h
)

# Create the executable
qt_add_executable(VEMCODE ${SOURCES} ${HEADERS})
set_target_properties(VEMCODE PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/app
)

# Include paths -- everything resolves from the project root
target_include_directories(VEMCODE PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core/runtime
    ${CMAKE_SOURCE_DIR}/src/core/host
    ${CMAKE_SOURCE_DIR}/src/ui
)

# Link Qt modules
target_link_libraries(VEMCODE PRIVATE
    Qt6::Core
    Qt6::Widgets
)

# Linux: link against dl for dlopen/dlsym used by SketchHost
if(UNIX AND NOT APPLE)
    target_link_libraries(VEMCODE PRIVATE dl)
endif()

# Windows-specific: hide the console window in release builds
# In debug builds we keep it so we can see std::cout output
if(WIN32)
    set_target_properties(VEMCODE PROPERTIES
        WIN32_EXECUTABLE $<CONFIG:Release>
    )
endif()