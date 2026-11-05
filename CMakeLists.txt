cmake_minimum_required(VERSION 3.16)
project(Kavaler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Network PrintSupport)

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
    src/import/TextUtil.cpp
    src/import/NameConverter.cpp
    src/import/ProfessionConverter.cpp
    src/import/DocumentParser.cpp
    src/import/DocxText.cpp
    src/import/PdfText.cpp
    src/import/Translator.cpp
    src/import/ImportDialog.cpp
    resources.qrc
)

target_include_directories(Kavaler PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/import"
    "${CMAKE_CURRENT_SOURCE_DIR}/PDFLib/Headers"
)
target_link_directories(Kavaler PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/PDFLib/Lib/x64")
target_link_libraries(Kavaler PRIVATE Qt6::Widgets Qt6::Sql Qt6::Network Qt6::PrintSupport PDFNetC)

add_custom_command(TARGET Kavaler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/PDFLib/Lib/x64/PDFNetC.dll"
        "$<TARGET_FILE_DIR:Kavaler>"
)
