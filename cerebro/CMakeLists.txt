#include_directories(./)
#add_subdirectory(hello)
#add_subdirectory(tushare)

#SUBDIRLIST(SUBDIRS ${CMAKE_CURRENT_SOURCE_DIR})
#message("storage ${SUBDIRS}")
#FOREACH(subdir ${SUBDIRS})
#    #gen_unit_test( ${subdir} )
#    add_subdirectory(${subdir})
#ENDFOREACH()

set(LIB_NAME cerebro)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

#file(GLOB_RECURSE LIB_STORAGE_INC ${CMAKE_CURRENT_SOURCE_DIR}/include/storage *.h)
#file(GLOB_RECURSE LIB_STORAGE_SRC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp)
file(GLOB_RECURSE LIB_INC  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h)

#add_subdirectory(src)

set(LIB_SRC cerebro_broker.cpp cerebro_struct.cpp cerebro_trader.cpp cerebro_quote.cpp cerebro_persist.cpp cerebro_backtest.cpp 
            sina_api.cpp tencent_api.cpp mdlink.cpp
            mysql_arrow.cpp mysql_connect.h
            )

add_library(${LIB_NAME} ${LIB_SRC} ${LIB_INC})
add_dependencies(${LIB_NAME} util)

set(APP_NAME cerebro_example)
add_executable(${APP_NAME} example.cpp)
target_link_libraries(${APP_NAME} ${LIB_NAME} util ${ARROW_LIBRARIES} ${GFLAGS_LIBRARIES} ${SNAPPY_LIBRARIES} ${LZ4_LIBRARIES} ${ZLIB_LIBRARIES} ${SYS_LIBS} ${MYSQL_LIBRARIES})
#message("LINK: ${SYS_LIBS}")

#target_include_directories(${APP_NAME} PUBLIC ../src)
#install(TARGETS ${APP_NAME} RUNTIME DESTINATION bin)
