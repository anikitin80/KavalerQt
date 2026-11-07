find_package(Qt6 REQUIRED COMPONENTS Test Sql)

# один исполняемый файл на тест; данные лежат в tests/data
function(kavaler_add_test name)
    add_executable(${name} ${name}.cpp TestData.h)
    target_link_libraries(${name} PRIVATE KavalerParser Qt6::Test)
    target_compile_definitions(${name} PRIVATE KAVALER_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")
    add_test(NAME ${name} COMMAND ${name})
    # DLL Qt рядом с тестами не копируются
    if(WIN32 AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.22)
        set_tests_properties(${name} PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    endif()
endfunction()

kavaler_add_test(tst_textutil)
kavaler_add_test(tst_nameconverter)
kavaler_add_test(tst_professionconverter)
kavaler_add_test(tst_documentparser)
kavaler_add_test(tst_decrees)
kavaler_add_test(tst_docxtext)

# каталог лежит в исходниках программы, не в библиотеке парсера
kavaler_add_test(tst_catalog)
target_sources(tst_catalog PRIVATE "${PROJECT_SOURCE_DIR}/src/Catalog.h" "${PROJECT_SOURCE_DIR}/src/Catalog.cpp"
    "${PROJECT_SOURCE_DIR}/src/import/ImportMatch.h" "${PROJECT_SOURCE_DIR}/src/import/ImportMatch.cpp")
target_include_directories(tst_catalog PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(tst_catalog PRIVATE Qt6::Sql)

# перевод Яндекса на местном сервере-заменителе
find_package(Qt6 REQUIRED COMPONENTS Network)
kavaler_add_test(tst_translator)
target_sources(tst_translator PRIVATE "${PROJECT_SOURCE_DIR}/src/import/Translator.h"
    "${PROJECT_SOURCE_DIR}/src/import/Translator.cpp"
    "${PROJECT_SOURCE_DIR}/src/Catalog.h" "${PROJECT_SOURCE_DIR}/src/Catalog.cpp"
    "${PROJECT_SOURCE_DIR}/src/import/ImportMatch.h" "${PROJECT_SOURCE_DIR}/src/import/ImportMatch.cpp")
target_include_directories(tst_translator PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(tst_translator PRIVATE Qt6::Sql Qt6::Network)
