option(WITH_CORE_DUMP "builds with support for core dump files (with GPL2 license)")
set(CRASH_STACK_BIN "crash-stack")
set(CRASH_STACK_SRCS crash-stack.c)

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "armv7l")
  set(CRASH_STACK_SRCS ${CRASH_STACK_SRCS} crash-stack-arm.c wind/unwarm.c wind/unwarm_thumb.c wind/unwarm_arm.c wind/unwarmmem.c)
else()
  set(CRASH_STACK_SRCS ${CRASH_STACK_SRCS} crash-stack-libelf.c)
endif()

add_executable(${CRASH_STACK_BIN} ${CRASH_STACK_SRCS})

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "armv7l")
  set_property(TARGET ${CRASH_STACK_BIN} APPEND_STRING PROPERTY COMPILE_FLAGS " -DUPGRADE_ARM_STACK_UNWIND")
#  set_property(TARGET ${CRASH_STACK_BIN} APPEND_STRING PROPERTY COMPILE_FLAGS "-DUPGRADE_ARM_STACK_UNWIND -DUNW_DEBUG")
endif()

if (${WITH_CORE_DUMP} STREQUAL "ON")
  set_property(TARGET ${CRASH_STACK_BIN} APPEND_STRING PROPERTY COMPILE_FLAGS " -DWITH_CORE_DUMP")
endif()

target_link_libraries(${CRASH_STACK_BIN} dw elf ebl dl stdc++)
install(TARGETS ${CRASH_STACK_BIN} DESTINATION libexec)
