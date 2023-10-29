cmake_minimum_required(VERSION 3.25)
project(webserver)

set(CMAKE_CXX_STANDARD 14)
include_directories("/usr/include/mysql")
link_directories("usr/lib/x86_64-linux-gnu")
set(MYSQL_LIBS
        mysqlclient pthread z m rt atomic ssl crypto dl
        )
target_link_libraries(${PROJECT_NAME}  ${MYSQL_LIBS})
include_directories(http)
include_directories(lock)
include_directories(log)
include_directories(sql)

add_executable(webserver
        http/http_conn.h
        lock/locker.h
        log/block_queue.h
        log/log.cpp
        log/log.h
        sql/sql_conn_pool.h sql/sql_conn_pool.cpp threadpool/threadpool.h threadpool/threadpool.h threadpool/threadpool.cpp timer/lst_timer.h timer/lst_timer.h timer/lst_timer.cpp http/http_conn.cpp http/http_conn.cpp)
