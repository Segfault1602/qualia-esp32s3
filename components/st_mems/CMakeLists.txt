idf_component_register()

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/StMems.cmake")
target_link_libraries(${COMPONENT_LIB} INTERFACE st_mems)
