# PS3 platform configuration for RSDKv4

if(PS3_PLATFORM_GUARD)
    return()
endif()
set(PS3_PLATFORM_GUARD TRUE)

add_executable(RetroEngine ${RETRO_FILES})

# PS3 SDK usually provides its own toolchain, so this is a template
# Users would typically use a toolchain file with CMake

set(COMPILE_OGG TRUE)
set(COMPILE_VORBIS TRUE)
set(DEP_PATH .)

if(RETRO_USE_HW_RENDER)
    target_compile_definitions(RetroEngine PRIVATE RETRO_USING_OPENGL=1)
    # PSGL is usually linked via the SDK's own mechanism
    # but we can add some hints here
    target_link_libraries(RetroEngine
        PSGL
        PSGLU
        cg
        cgGL
        gcm_cmd
        gcm_sys_stub
    )
endif()

# PS3 doesn't typically use SDL2 out of the box unless using a port
# If using a port, the user should adjust this
if(RETRO_SDL_VERSION STREQUAL "2")
    # target_link_libraries(RetroEngine SDL2)
elseif(RETRO_SDL_VERSION STREQUAL "1")
    # target_link_libraries(RetroEngine SDL)
endif()

target_compile_definitions(RetroEngine PRIVATE
    PS3
    __PS3__
    __CELLOS_LV2__
)

# PS3 specific flags
target_compile_options(RetroEngine PRIVATE
    -maltivec
    -fno-exceptions
    -fno-rtti
)

target_link_libraries(RetroEngine
    audio_stub
    io_stub
    rsx_stub
    sysmodule_stub
    net_stub
)

if(RETRO_MOD_LOADER)
    set_target_properties(RetroEngine PROPERTIES
        CXX_STANDARD 11 # PS3 SDK 4.75 might have limited C++17 support
        CXX_STANDARD_REQUIRED ON
    )
endif()
