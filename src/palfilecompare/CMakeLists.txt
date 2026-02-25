set(LIB_NAME palfilecompare)

file(GLOB SRC_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.txt
)

add_executable(${LIB_NAME} ${SRC_LIST})
target_include_directories(${LIB_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${LIB_NAME} PRIVATE priceactionlab)