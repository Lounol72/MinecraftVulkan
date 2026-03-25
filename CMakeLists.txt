cmake_minimum_required(VERSION 3.20)
project(VulkanEngine CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# ── Sources ────────────────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# Exclude main.cpp from the library — only used in standalone mode
list(FILTER SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

# ── Library target (utilisable comme submodule) ────────────────────────────────
add_library(VulkanEngine STATIC ${SOURCES})

# Collecte tous les sous-dossiers de libs/ récursivement
file(GLOB_RECURSE LIB_SUBDIRS LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/libs/*)
list(FILTER LIB_SUBDIRS INCLUDE REGEX ".*/$|[^.]+$")
foreach(ENTRY ${LIB_SUBDIRS})
    if(IS_DIRECTORY ${ENTRY})
        list(APPEND LIB_INCLUDE_DIRS ${ENTRY})
    endif()
endforeach()

file(GLOB_RECURSE INCLUDE_SUBDIRS LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/include/*)
foreach(ENTRY ${INCLUDE_SUBDIRS})
    if(IS_DIRECTORY ${ENTRY})
        list(APPEND INCLUDE_DIRS ${ENTRY})
    endif()
endforeach()

target_include_directories(VulkanEngine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${INCLUDE_DIRS}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs>
    ${LIB_INCLUDE_DIRS}
    $<INSTALL_INTERFACE:include>
)

# ── Dependencies ───────────────────────────────────────────────────────────────
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

target_link_libraries(VulkanEngine PUBLIC
    Vulkan::Vulkan
    glfw
    dl
    pthread
    X11
    Xxf86vm
    Xrandr
    Xi
)

# ── Compiler flags ─────────────────────────────────────────────────────────────
target_compile_options(VulkanEngine PRIVATE
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:Debug>:-O0 -g>
)

target_compile_definitions(VulkanEngine PUBLIC
    $<$<CONFIG:Release>:NDEBUG>
)

# ── Shaders ────────────────────────────────────────────────────────────────────
find_program(GLSLC glslc REQUIRED)

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
file(GLOB SHADER_SOURCES
    ${SHADER_DIR}/*.vert
    ${SHADER_DIR}/*.frag
    ${SHADER_DIR}/*.comp
)

foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPV_OUTPUT ${SHADER_DIR}/${SHADER_NAME}.spv)

    add_custom_command(
        OUTPUT  ${SPV_OUTPUT}
        COMMAND ${GLSLC} ${SHADER} -o ${SPV_OUTPUT}
        DEPENDS ${SHADER}
        COMMENT "Compilation shader: ${SHADER_NAME}"
    )
    list(APPEND SPV_OUTPUTS ${SPV_OUTPUT})
endforeach()

add_custom_target(VulkanEngineShaders ALL DEPENDS ${SPV_OUTPUTS})
add_dependencies(VulkanEngine VulkanEngineShaders)

# ── Standalone executable (désactivé quand utilisé comme submodule) ────────────
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_executable(VulkanTest ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
    target_link_libraries(VulkanTest PRIVATE VulkanEngine)
endif()
