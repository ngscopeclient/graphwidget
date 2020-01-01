include_directories(${GTKMM_INCLUDE_DIRS} ${SIGCXX_INCLUDE_DIRS})
link_directories(${GTKMM_LIBRARY_DIRS} ${SIGCXX_LIBRARY_DIRS})

set(GRAPHWIDGET_SOURCES
	Graph.cpp
	)

add_library(graphwidget SHARED
	${GRAPHWIDGET_SOURCES})
target_link_libraries(graphwidget ${SIGCXX_LIBRARIES} ${GTKMM_LIBRARIES})

target_include_directories(graphwidget
PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS graphwidget LIBRARY DESTINATION /usr/lib)
