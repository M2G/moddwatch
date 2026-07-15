cmake_minimum_required(VERSION 4.2)
project(mechanism_computer_systems2)

set(CMAKE_CXX_STANDARD 20)

add_executable(
        mechanism_computer_systems2 main.cpp
        pthread/pthread_mutex.cpp
        pthread/pthread_multiple_threads.cpp
        pthread/pthread_join.cpp
        pthread/pthread_exit.cpp
        pthread/pthread_create.cpp
        pthread/pthread_race_condition.cpp
        pthread/pthread_trylock.cpp
)
