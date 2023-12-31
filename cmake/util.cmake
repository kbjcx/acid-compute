function(acid_add_test dir target)
    aux_source_directory(${PROJECT_SOURCE_DIR}/${dir} SRC_LIST_${target})
    set(LINK_ARGS ${LINK_ARGS} acid lua)
    foreach (var ${SRC_LIST_${target}})
        string(REGEX REPLACE ${PROJECT_SOURCE_DIR} "." src ${var})
        string(REGEX REPLACE ".*/" "" tgt ${src})
        string(REGEX REPLACE ".cpp" "" tgt ${tgt})
        add_executable(${tgt} ${src} )
        target_link_libraries(${tgt} -Wl,--start-group ${LINK_ARGS} -Wl,--end-group)
        target_link_libraries(${tgt} lua)
    endforeach ()
endfunction()