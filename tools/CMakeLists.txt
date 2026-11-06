# command line tools for checking the parser on the whole decree archive
find_package(Qt6 REQUIRED COMPONENTS Sql)

add_executable(decree_report
    DecreeReport.cpp
    "${PROJECT_SOURCE_DIR}/src/Catalog.h"
    "${PROJECT_SOURCE_DIR}/src/Catalog.cpp"
)
target_include_directories(decree_report PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(decree_report PRIVATE KavalerParser Qt6::Sql)
