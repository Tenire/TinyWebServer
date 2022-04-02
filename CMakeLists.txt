cmake_minimum_required(VERSION 3.21)
project(server)

set(CMAKE_CXX_STANDARD 17)

include_directories(CGImysql)
include_directories(http)
include_directories(lock)
include_directories(log)
include_directories(threadpool)
include_directories(timer)
include_directories(/usr/include)

add_executable(server
        CGImysql/sql_connection_pool.cpp
        CGImysql/sql_connection_pool.h
        http/http_conn.cpp
        http/http_conn.h
        lock/locker.h
        log/block_queue.h
        log/log.cpp
        log/log.h
        threadpool/threadpool.h
        timer/lst_timer.h
        main.cpp Notes.md)
target_link_libraries(server -lpthread -lmysqlclient)