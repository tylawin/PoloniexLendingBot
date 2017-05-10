#SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Binaries)
#SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Binaries)

#SET(Boost_DEBUG 1)
SET(Boost_USE_STATIC_LIBS ON)

SET(CPPREST_EXCLUDE_WEBSOCKETS ON)
SET(BUILD_TESTS OFF)
SET(BUILD_SAMPLES OFF)
ADD_SUBDIRECTORY(submodules/cpprestsdk/Release)

PROJECT(PoloniexLendingBot)

IF(MSVC)#windows msvc2017
	CMAKE_MINIMUM_REQUIRED(VERSION 3.8.1)
	
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /bigobj")
	ADD_DEFINITIONS(-DBOOST_ALL_NO_LIB)
	
	SET(BOOST_ROOT D:/Programming/Libraries/boost_1_64_0)
	SET(BOOST_LIBRARYDIR D:/Programming/Libraries/boost_1_64_0/stage/lib)
ELSE()#raspberry pi (raspbian)
	CMAKE_MINIMUM_REQUIRED(VERSION 3.0.2)
	
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")
	
	#SET(BOOST_INCLUDEDIR /usr/include/boost)
	#SET(BOOST_LIBRARYDIR /usr/lib/arm-linux-gnueabihf)
ENDIF()

FIND_PACKAGE(Boost 1.55 REQUIRED filesystem date_time log)

ADD_LIBRARY(hmac STATIC submodules/hmac/sha2.c submodules/hmac/hmac_sha2.c)

ADD_SUBDIRECTORY(source)

TARGET_INCLUDE_DIRECTORIES(PoloLendingBot PUBLIC include submodules/Decimal/include submodules/cpprestsdk/Release/include submodules/hmac ${Boost_INCLUDE_DIR})
