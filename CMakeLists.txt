CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(crash-worker C CXX)

SET(PREFIX ${CMAKE_INSTALL_PREFIX})

# Sub modules
ADD_SUBDIRECTORY(src/crash-manager)
ADD_SUBDIRECTORY(src/crash-pipe)
ADD_SUBDIRECTORY(src/crash-stack)
ADD_SUBDIRECTORY(src/dump_systemstate)

