#SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Binaries)
#SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Binaries)

#SET(Boost_DEBUG 1)
SET(Boost_USE_STATIC_LIBS ON)

SET(CPPREST_EXCLUDE_WEBSOCKETS ON)
SET(BUILD_TESTS OFF)
SET(BUILD_SAMPLES OFF)
ADD_SUBDIRECTORY(submodules/cpprestsdk/Release)
#LINK_DIRECTORIES(D:/Programming/Git/tylawin/PoloLendingBot/submodules/cpprestsdk/Release/Binaries)
#LINK_DIRECTORIES(D:/Programming/Libraries/boost_1_59_0/lib64-msvc-14)

CMAKE_MINIMUM_REQUIRED(VERSION 2.8.11)
PROJECT(PoloniexLendingBot)

#add_definitions(-DBOOST_ALL_NO_LIB)
#SET(BOOST_ROOT D:/Programming/Libraries/boost_1_59_0)
#SET(BOOST_LIBRARYDIR D:/Programming/Libraries/boost_1_59_0/lib64-msvc-14.0)
SET(BOOST_INCLUDEDIR /usr/include/boost)
SET(BOOST_LIBRARYDIR /usr/lib/arm-linux-gnueabihf)
FIND_PACKAGE(Boost 1.55 REQUIRED filesystem date_time log)
#SET(OPENSSL_ROOT_DIR D:/Programming/Libraries/built_openssl)
#FIND_PACKAGE(OpenSSL)


IF(MSVC)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /bigobj")
ELSE()
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")
ENDIF()


ADD_SUBDIRECTORY(submodules/hmac)
ADD_LIBRARY(hmac STATIC submodules/hmac/sha2.c submodules/hmac/hmac_sha2.c)


ADD_SUBDIRECTORY(source)

TARGET_INCLUDE_DIRECTORIES(PoloLendingBot PUBLIC include submodules/Decimal/include submodules/cpprestsdk/Release/include submodules/hmac ${Boost_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
