# MrpackMaker — Qt6 GUI mrpack 製作工具
# 編譯環境：Qt 6.11.0 MSVC2022（與既有 MCServerManager / WorldGenModule 專案一致）
#
# 依賴說明：
#   - QuaZip：比照 WorldGenModule 的做法，手動編譯安裝，不透過 vcpkg 的 quazip port
#     （vcpkg 官方 quazip@1.5 沒有 qt6 feature，且會強制拉自己的 qtbase，
#      跟另外安裝的 Qt6 衝突）。請依你自己的實際安裝路徑調整下方 QuaZip_DIR。
#   - OpenSSL：沿用 MCServer-Manager-17 的 third_party/openssl（libcrypto 用來算 sha1/sha512）。
#     若沒有把 third_party/openssl 複製過來，改成系統安裝的 OpenSSL 亦可，
#     只要 find_package(OpenSSL REQUIRED) 抓得到即可。
#   - vcpkg.json 僅列 zlib（QuaZip 內部需要），沒有列 quazip 本身；bzip2 保留給未來若有
#     其他用途時可用，本工具的 .mrpack 匯出本身固定使用標準 Deflate 壓縮
#     （原因見 src/core/MrpackBuilder.h 開頭的說明：mrpack 官方規格要求標準 ZIP，
#      用 bzip2 當 zip entry 壓縮方法會讓其他啟動器讀不到）。
#   - CurseForge API Key 加密儲存（src/security/）沿用 MCServer-Manager-17 的做法：
#     RSA-2048 + AES-256-GCM 混合加密內容，私鑰另外用 Windows DPAPI 加密後才落地存檔，
#     金鑰檔路徑記錄在登錄檔（HKCU\Software\MrpackMaker）。這一整套機制用到
#     windows.h / wincrypt.h / QSettings(NativeFormat)，只能在 Windows 上編譯執行，
#     跟本專案原本鎖定 MSVC2022 + Windows 的前提一致。

cmake_minimum_required(VERSION 3.16)
project(MrpackMaker)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent)

# ---------------------------------------------------------------------------
# QuaZip（手動編譯安裝版本，路徑請依實際環境調整）
# ---------------------------------------------------------------------------
if(NOT DEFINED QuaZip_DIR AND DEFINED ENV{QUAZIP_INSTALL_DIR})
    set(QuaZip_DIR "$ENV{QUAZIP_INSTALL_DIR}/lib/cmake/QuaZip-Qt6-1.5")
endif()
find_package(QuaZip-Qt6 REQUIRED)

# ---------------------------------------------------------------------------
# OpenSSL（用於 HashCalculator 計算 sha1 / sha512）
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/openssl")
    set(OPENSSL_ROOT_DIR "${CMAKE_SOURCE_DIR}/third_party/openssl")
endif()
find_package(OpenSSL REQUIRED)

set(APP_SOURCES
    src/main.cpp
    src/ModloaderInfo.h

    src/core/ModpackData.h
    src/core/HashCalculator.h
    src/core/HashCalculator.cpp
    src/core/FileDownloader.h
    src/core/FileDownloader.cpp
    src/core/MrpackBuilder.h
    src/core/MrpackBuilder.cpp

    src/network/VersionFetcher.h
    src/network/VersionFetcher.cpp
    src/network/ModrinthApi.h
    src/network/ModrinthApi.cpp
    src/network/CurseForgeApi.h
    src/network/CurseForgeApi.cpp

    src/security/RsaCrypto.h
    src/security/RsaCrypto.cpp
    src/security/DpapiProtector.h
    src/security/DpapiProtector.cpp
    src/security/RegistryKeyStore.h
    src/security/RegistryKeyStore.cpp
    src/security/KeyPathGenerator.h
    src/security/KeyPathGenerator.cpp
    src/security/CurseForgeSecureStorage.h
    src/security/CurseForgeSecureStorage.cpp

    src/ui/MrpackWizard.h
    src/ui/MrpackWizard.cpp
    src/ui/BasicInfoPage.h
    src/ui/BasicInfoPage.cpp
    src/ui/FilesPage.h
    src/ui/FilesPage.cpp
    src/ui/ModSearchDialog.h
    src/ui/ModSearchDialog.cpp
    src/ui/OverridesPage.h
    src/ui/OverridesPage.cpp
    src/ui/OverrideTreeSelector.h
    src/ui/OverrideTreeSelector.cpp
    src/ui/ExportPage.h
    src/ui/ExportPage.cpp

    resources/resources.qrc
)

if(WIN32)
    if(EXISTS "${CMAKE_SOURCE_DIR}/resources/app.rc")
        list(APPEND APP_SOURCES resources/app.rc)
    endif()
endif()

add_executable(MrpackMaker WIN32 ${APP_SOURCES})

target_include_directories(MrpackMaker PRIVATE
    src
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(MrpackMaker PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Concurrent
    QuaZip::QuaZip
    OpenSSL::Crypto
)

if(WIN32)
    target_link_libraries(MrpackMaker PRIVATE ws2_32 crypt32)
endif()

# 安裝設定（可選）：把執行檔與必要 DLL 收集到同一個輸出資料夾，方便直接發布
install(TARGETS MrpackMaker RUNTIME DESTINATION .)
