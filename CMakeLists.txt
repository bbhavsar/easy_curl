cmake_minimum_required(VERSION 3.19)
project(easy_curl)

set(CMAKE_CXX_STANDARD 17)

add_library(easy_curl SHARED easy_curl.cpp easy_curl.h)
target_link_libraries(easy_curl curl)
install(TARGETS easy_curl DESTINATION lib)
install(FILES easy_curl.h DESTINATION include)
