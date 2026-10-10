include_guard(GLOBAL)
include(FetchContent)

set(BUILD_SHARED_LIBS OFF)
# ESP-IDF configures components such that Box2D sees itself as the top-level
# project, which would otherwise enable its samples, tests, and validation.
set(BOX2D_SAMPLES OFF)
set(BOX2D_BENCHMARKS OFF)
set(BOX2D_DOCS OFF)
set(BOX2D_PROFILE OFF)
set(BOX2D_VALIDATE OFF)
set(BOX2D_UNIT_TESTS OFF)

FetchContent_Declare(box2d
    URL https://github.com/erincatto/box2d/archive/8c661469c9507d3ad6fbd2fea3f1aa71669c2fe3.tar.gz
    URL_HASH SHA256=dee7bcf0f50b3dcccf67678ce14ffe3ce02583b51b8b03e447ab30a84c3436ef
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(box2d)

if(ESP_PLATFORM)
    # One world keeps Box2D's static world table small; asserts follow release builds.
    target_compile_definitions(box2d PRIVATE NDEBUG B2_MAX_WORLDS=1)
    # Upstream also disables -Wunused-value for GCC; its assert macros trip it.
    target_compile_options(box2d PRIVATE -std=gnu17 -Wno-unused-value -Wno-error=format)
endif()
