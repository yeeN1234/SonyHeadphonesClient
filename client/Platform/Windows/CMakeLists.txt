cmake_minimum_required(VERSION 3.31)

add_library(Client_PlatformWindows STATIC
        ClientPlatform.cpp
        Tray.cpp
)

target_link_libraries(Client_PlatformWindows
        PUBLIC
        mdr::bt
        PRIVATE
        mdr
        ws2_32
        rpcrt4
        user32
        gdi32
        shell32
        gdiplus
        advapi32
)
