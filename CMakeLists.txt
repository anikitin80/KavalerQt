cmake_minimum_required(VERSION 3.16)
project(Kavaler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(KAVALER_BUILD_TESTS "Build the decree parser tests" ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql Network PrintSupport)

# парсер указов: текст docx → люди; без интерфейса, общий с тестами
add_library(KavalerParser STATIC
    src/import/TextUtil.h
    src/import/TextUtil.cpp
    src/import/ParserTypes.h
    src/import/NameConverter.h
    src/import/NameConverter.cpp
    src/import/RankConverter.h
    src/import/RankConverter.cpp
    src/import/ProfessionConverter.h
    src/import/ProfessionConverter.cpp
    src/import/DocumentParser.h
    src/import/DocumentParser.cpp
    src/import/DocxText.h
    src/import/DocxText.cpp
)
target_include_directories(KavalerParser PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/import")
# Qt6::CorePrivate: QZipReader для чтения .docx (DocxText.cpp)
target_link_libraries(KavalerParser PUBLIC Qt6::Core PRIVATE Qt6::CorePrivate)

add_executable(Kavaler WIN32
    src/main.cpp
    src/Catalog.h
    src/Catalog.cpp
    src/Models.h
    src/Models.cpp
    src/EditDialog.h
    src/EditDialog.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/import/Translator.cpp
    src/import/ImportDialog.cpp
    src/import/ImportMatch.h
    src/import/ImportMatch.cpp
    resources.qrc
)

target_include_directories(Kavaler PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/import"
)
target_link_libraries(Kavaler PRIVATE KavalerParser Qt6::Widgets Qt6::Sql Qt6::Network Qt6::PrintSupport)

# cmake --install build --config Release --prefix <папка>: папка, которая запускается
# на машине без Qt. Всё лежит рядом с Kavaler.exe: библиотеки Qt и
# плагины (драйвер SQLite, TLS для перевода Яндекса, печать) из
# windeployqt и среда MSVC. Программа переводы Qt не загружает.
set(CMAKE_INSTALL_BINDIR ".")
install(TARGETS Kavaler RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
# файл развёртывания Qt по умолчанию кладёт всё в bin/; плагины Qt идут в plugins/ (qt.conf)
install(CODE "set(QT_DEPLOY_BIN_DIR \".\")")
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.5)
    qt_generate_deploy_app_script(TARGET Kavaler OUTPUT_SCRIPT kavaler_deploy_script
        NO_TRANSLATIONS NO_UNSUPPORTED_PLATFORM_ERROR
        DEPLOY_TOOL_OPTIONS --exclude-plugins qsqlmimer,qsqlodbc,qsqlpsql)
else()
    qt_generate_deploy_app_script(TARGET Kavaler FILENAME_VARIABLE kavaler_deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR)
endif()
install(SCRIPT "${kavaler_deploy_script}")
if(MSVC)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${CMAKE_INSTALL_BINDIR}")
    include(InstallRequiredSystemLibraries)
endif()

if(KAVALER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

option(KAVALER_BUILD_TOOLS "Build the command line tools (decree_report)" ON)
if(KAVALER_BUILD_TOOLS)
    add_subdirectory(tools)
endif()
