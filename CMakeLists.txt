cmake_minimum_required(VERSION 2.8.9)

project(exp1)
 
# -O2 will make the race condition difficult to manifest. but it's there!
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall")

include_directories(
  ./
)

file(GLOB HEADERS
"./*.h"
)

file(GLOB COMMON_SOURCES
  ${HEADERS}
  # mytar.c 
  inodemap.c 
)

add_executable(mytar
  mytar.c
  ${COMMON_SOURCES}
)


 
    


