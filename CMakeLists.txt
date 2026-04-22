cmake_minimum_required(VERSION 3.16)

project(AFormParserMinimal LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)



find_package(Qt6 QUIET COMPONENTS Core)
if (Qt6_FOUND)
    set(QT_PACKAGE Qt6)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
else()
    find_package(Qt5 REQUIRED COMPONENTS Core Widgets)
    set(QT_PACKAGE Qt5)
endif()

set(LUA_SOURCES
    thirdparty/lua/lapi.c
    thirdparty/lua/lcode.c
    thirdparty/lua/lctype.c
    thirdparty/lua/ldebug.c
    thirdparty/lua/ldo.c
    thirdparty/lua/ldump.c
    thirdparty/lua/lfunc.c
    thirdparty/lua/lgc.c
    thirdparty/lua/llex.c
    thirdparty/lua/lmem.c
    thirdparty/lua/lobject.c
    thirdparty/lua/lopcodes.c
    thirdparty/lua/lparser.c
    thirdparty/lua/lstate.c
    thirdparty/lua/lstring.c
    thirdparty/lua/ltable.c
    thirdparty/lua/ltm.c
    thirdparty/lua/lundump.c
    thirdparty/lua/lzio.c
    thirdparty/lua/lvm.c
    thirdparty/lua/lauxlib.c
    thirdparty/lua/lbaselib.c
    thirdparty/lua/lcorolib.c
    thirdparty/lua/ldblib.c
    thirdparty/lua/liolib.c
    thirdparty/lua/lmathlib.c
    thirdparty/lua/loslib.c
    thirdparty/lua/lstrlib.c
    thirdparty/lua/ltablib.c
    thirdparty/lua/lutf8lib.c
    thirdparty/lua/loadlib.c
    thirdparty/lua/linit.c
)

add_library(aformparser STATIC
    src/AFormParser.cpp
    src/LuaRuntime.cpp
    include/AFormParser/AFormParser.hpp
    include/AFormParser/LuaRuntime.hpp
    ${LUA_SOURCES}
)

target_include_directories(aformparser PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua
)

target_link_libraries(aformparser PUBLIC
    ${QT_PACKAGE}::Core
)

add_executable(legacy_to_form
    tools/legacy_to_form/main.cpp
)

target_link_libraries(legacy_to_form PRIVATE
    aformparser
    ${QT_PACKAGE}::Core
)

add_executable(verify_form_features
    tests/verify_form_features.cpp
)

target_link_libraries(verify_form_features PRIVATE
    aformparser
    ${QT_PACKAGE}::Core
)

add_executable(form_to_cfg
    tools/form_to_cfg/form_to_cfg.cpp
)

target_link_libraries(form_to_cfg PRIVATE
    aformparser
    ${QT_PACKAGE}::Core
)

add_executable(form_tree_viewer
    tools/form_tree_viewer/form_tree_viewer.cpp
)

target_link_libraries(form_tree_viewer PRIVATE
    aformparser
    ${QT_PACKAGE}::Core
    ${QT_PACKAGE}::Widgets
)

add_executable(register_function_demo
    tools/register_function_demo/register_function_demo.cpp
)

target_link_libraries(register_function_demo PRIVATE
    aformparser
    ${QT_PACKAGE}::Core
)

if (WIN32)
    add_custom_command(TARGET legacy_to_form POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${QT_PACKAGE}::Core>
            $<TARGET_FILE_DIR:legacy_to_form>
    )

    add_custom_command(TARGET verify_form_features POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${QT_PACKAGE}::Core>
            $<TARGET_FILE_DIR:verify_form_features>
    )

    add_custom_command(TARGET form_to_cfg POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${QT_PACKAGE}::Core>
            $<TARGET_FILE_DIR:form_to_cfg>
    )

    add_custom_command(TARGET form_tree_viewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${QT_PACKAGE}::Core>
            $<TARGET_FILE_DIR:form_tree_viewer>
    )

    add_custom_command(TARGET register_function_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${QT_PACKAGE}::Core>
            $<TARGET_FILE_DIR:register_function_demo>
    )

    add_custom_command(TARGET form_tree_viewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${QT_PACKAGE}::Widgets>
            $<TARGET_FILE_DIR:form_tree_viewer>
    )

    if (QT_PACKAGE STREQUAL "Qt6")
        get_target_property(_qt_qmake Qt6::qmake IMPORTED_LOCATION)
    else()
        get_target_property(_qt_qmake Qt5::qmake IMPORTED_LOCATION)
    endif()

    if (_qt_qmake)
        get_filename_component(_qt_bin_dir "${_qt_qmake}" DIRECTORY)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
        if (WINDEPLOYQT_EXECUTABLE)
            add_custom_command(TARGET form_tree_viewer POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations --dir "$<TARGET_FILE_DIR:form_tree_viewer>" "$<TARGET_FILE:form_tree_viewer>"
                VERBATIM
            )
        endif()
    endif()
endif()
