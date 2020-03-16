CMAKE_MINIMUM_REQUIRED(VERSION 3.11)
PROJECT(wget)

SET(DEFAULT_BUILD_TYPE "Debug")
IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
ENDIF()
SET(EXPECTED_BUILD_TYPES Debug Release RelWithDebInfo MinSizeRel)
IF("${CMAKE_BUILD_TYPE}" IN_LIST EXPECTED_BUILD_TYPES)
    MESSAGE(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
ELSE()
    MESSAGE(WARNING "Unexcpected build type: ${CMAKE_BUILD_TYPE}")
ENDIF()

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_C_STANDARD 17)
SET(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -Werror")
SET(CMAKE_C_FLAGS "-Wall -Wextra -Wpedantic -Werror")

INCLUDE_DIRECTORIES(.)

SET(HTTP_RESP_FILES HttpResponseParser.cpp HttpResponseParser.hpp)
SET(UTILS_FILES MakeArray.hpp)

SET(SOURCE_FILES main.cpp ${HTTP_RESP_FILES})

ADD_EXECUTABLE(wget ${SOURCE_FILES})

ADD_EXECUTABLE(HttpResponseParserUnitTest HttpResponseParserUnitTest.cpp ${HTTP_RESP_FILES})
ADD_EXECUTABLE(HttpResponseParserPerfTest HttpResponseParserPerfTest.cpp ${HTTP_RESP_FILES})
ADD_EXECUTABLE(MakeArrayUnitTest MakeArrayUnitTest.cpp ${UTILS_FILES})

ENABLE_TESTING()
ADD_TEST(NAME HttpResponseParserUnitTest COMMAND HttpResponseParserUnitTest)
ADD_TEST(NAME MakeArrayUnitTest COMMAND MakeArrayUnitTest)
