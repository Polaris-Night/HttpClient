set(LIB_NAME http_client)

file(GLOB SRCS ${CMAKE_CURRENT_LIST_DIR}/*.cpp ${CMAKE_CURRENT_LIST_DIR}/*.h)

add_library(${LIB_NAME} STATIC ${SRCS})
target_link_libraries(${LIB_NAME} PRIVATE event_core event_extra event_pthreads)
target_include_directories(${LIB_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR} PRIVATE ${CMAKE_CURRENT_LIST_DIR})

if (BUILD_WITH_SSL)
    target_compile_definitions(${LIB_NAME} PRIVATE BUILD_WITH_SSL)
    target_link_libraries(${LIB_NAME} PRIVATE event_openssl)
endif()