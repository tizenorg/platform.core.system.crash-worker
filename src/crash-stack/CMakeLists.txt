set(CRASH_STACK_BIN "crash-stack")
set(CRASH_STACK_SRCS crash-stack.c)

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "armv7l")
  set(CRASH_STACK_SRCS ${CRASH_STACK_SRCS} crash-stack-arm.c wind/unwarm.c wind/unwarm_thumb.c wind/unwarm_arm.c wind/unwarmmem.c)
else()
  set(CRASH_STACK_SRCS ${CRASH_STACK_SRCS} crash-stack-libelf.c)
endif()

add_executable(${CRASH_STACK_BIN} ${CRASH_STACK_SRCS})
set_property(TARGET ${CRASH_STACK_BIN} APPEND PROPERTY COMPILE_FLAGS "-DUPGRADE_ARM_STACK_UNWIND")
#set_property(TARGET ${CRASH_STACK_BIN} APPEND PROPERTY COMPILE_FLAGS "-DUPGRADE_ARM_STACK_UNWIND -DUNW_DEBUG")
target_link_libraries(${CRASH_STACK_BIN} dw elf ebl dl stdc++)
install(TARGETS ${CRASH_STACK_BIN} DESTINATION libexec)
