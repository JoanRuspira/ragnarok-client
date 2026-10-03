if(NOT DEFINED QT_DEPLOY_TARGET_FILE OR QT_DEPLOY_TARGET_FILE STREQUAL "")
    message(FATAL_ERROR "QT_DEPLOY_TARGET_FILE is required")
endif()

if(NOT DEFINED QT_DEPLOY_OUTPUT_DIR OR QT_DEPLOY_OUTPUT_DIR STREQUAL "")
    message(FATAL_ERROR "QT_DEPLOY_OUTPUT_DIR is required")
endif()

if(NOT DEFINED QT_DEPLOY_WINDEPLOYQT OR QT_DEPLOY_WINDEPLOYQT STREQUAL "")
    message(FATAL_ERROR "QT_DEPLOY_WINDEPLOYQT is required")
endif()

if(NOT EXISTS "${QT_DEPLOY_WINDEPLOYQT}")
    message(FATAL_ERROR "windeployqt not found at '${QT_DEPLOY_WINDEPLOYQT}'")
endif()

file(MAKE_DIRECTORY "${QT_DEPLOY_OUTPUT_DIR}")
get_filename_component(_qt_target_name "${QT_DEPLOY_TARGET_FILE}" NAME)
set(_qt_deployed_target "${QT_DEPLOY_OUTPUT_DIR}/${_qt_target_name}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${QT_DEPLOY_TARGET_FILE}"
        "${_qt_deployed_target}"
    RESULT_VARIABLE _copy_result
)
if(NOT _copy_result EQUAL 0)
    message(FATAL_ERROR "Failed to copy target executable into '${QT_DEPLOY_OUTPUT_DIR}'")
endif()

execute_process(
    COMMAND "${QT_DEPLOY_WINDEPLOYQT}"
        --release
        --dir "${QT_DEPLOY_OUTPUT_DIR}"
        --qmldir "${QT_DEPLOY_QML_DIR}"
        "${_qt_deployed_target}"
    RESULT_VARIABLE _deploy_result
)
if(NOT _deploy_result EQUAL 0)
    message(FATAL_ERROR "windeployqt failed for '${QT_DEPLOY_OUTPUT_DIR}'")
endif()

set(_qt_runtime_root "${QT_DEPLOY_OUTPUT_DIR}/thirdparty/qt")
file(MAKE_DIRECTORY "${_qt_runtime_root}")

set(_qt_runtime_dirs
    generic
    iconengines
    imageformats
    networkinformation
    platforms
    qml
    qmltooling
    sqldrivers
    styles
    tls
    translations
)

foreach(_qt_runtime_dir IN LISTS _qt_runtime_dirs)
    set(_qt_runtime_source "${QT_DEPLOY_OUTPUT_DIR}/${_qt_runtime_dir}")
    if(EXISTS "${_qt_runtime_source}")
        set(_qt_runtime_dest "${_qt_runtime_root}/${_qt_runtime_dir}")
        if(EXISTS "${_qt_runtime_dest}")
            file(REMOVE_RECURSE "${_qt_runtime_dest}")
        endif()
        file(RENAME "${_qt_runtime_source}" "${_qt_runtime_dest}")
    endif()
endforeach()

file(WRITE "${QT_DEPLOY_OUTPUT_DIR}/qt.conf" [=[
[Paths]
Prefix = thirdparty/qt
Plugins = .
Imports = qml
Qml2Imports = qml
Translations = translations
]=])

if(DEFINED QT_DEPLOY_LICENSES_DIR AND EXISTS "${QT_DEPLOY_LICENSES_DIR}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${QT_DEPLOY_OUTPUT_DIR}/licenses"
        RESULT_VARIABLE _mkdir_result
    )
    if(NOT _mkdir_result EQUAL 0)
        message(FATAL_ERROR "Failed to create licenses directory in '${QT_DEPLOY_OUTPUT_DIR}'")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${QT_DEPLOY_LICENSES_DIR}"
            "${QT_DEPLOY_OUTPUT_DIR}/licenses/qt"
        RESULT_VARIABLE _license_copy_result
    )
    if(NOT _license_copy_result EQUAL 0)
        message(FATAL_ERROR "Failed to copy Qt license notices into '${QT_DEPLOY_OUTPUT_DIR}'")
    endif()
endif()
