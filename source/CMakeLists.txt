ADD_EXECUTABLE(PoloLendingBot PoloLendingBot.cpp)

SET_TARGET_PROPERTIES(PoloLendingBot PROPERTIES INTERFACE_LINK_LIBRARIES cpprest)

IF(MSVC)
	SET(LINK_LIBRARY_CPPREST optimized ${CMAKE_BINARY_DIR}/submodules/cpprestsdk/Release/Binaries/Release/cpprest_2_8.lib debug ${CMAKE_BINARY_DIR}/submodules/cpprestsdk/Release/Binaries/Debug/cpprest_2_8.lib)
ELSE()
	SET(LINK_LIBRARY_CPPREST ${CMAKE_BINARY_DIR}/submodules/cpprestsdk/Release/Binaries/libcpprest.so)
ENDIF()

FIND_PACKAGE(Threads REQUIRED)
IF(THREADS_HAVE_PTHREAD_ARG)
	TARGET_COMPILE_OPTIONS(PUBLIC PoloLendingBot "-pthread")
ENDIF()

TARGET_LINK_LIBRARIES(PoloLendingBot hmac ${Boost_LIBRARIES} ${LINK_LIBRARY_CPPREST})
IF(CMAKE_THREAD_LIBS_INIT)
	TARGET_LINK_LIBRARIES(PoloLendingBot "${CMAKE_THREAD_LIBS_INIT}")
ENDIF()

IF(NOT MSVC) #without websocket enabled windows compile doesn't need openssl
	FIND_PACKAGE(OpenSSL REQUIRED)
	TARGET_LINK_LIBRARIES(PoloLendingBot "${OPENSSL_LIBRARIES}")
ENDIF()

IF(MSVC)
	ADD_CUSTOM_COMMAND(TARGET PoloLendingBot POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_BINARY_DIR}/submodules/cpprestsdk/Release/Binaries/$<CONFIGURATION>" $<TARGET_FILE_DIR:PoloLendingBot>)
ENDIF()

SET_PROPERTY(TARGET PoloLendingBot PROPERTY FOLDER "executables")

INSTALL(TARGETS PoloLendingBot RUNTIME DESTINATION ${PROJECT_BINARY_DIR}/bin)
