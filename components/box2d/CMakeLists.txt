idf_component_register()

# Keep SDK link dependencies out of Box2D's install export.
set_property(DIRECTORY PROPERTY LINK_LIBRARIES "")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/Box2D.cmake")
target_link_libraries(${COMPONENT_LIB} INTERFACE box2d)
