set(CRASH_STACK_BIN "crash-stack")
set(CRASH_STACK_SRCS crash-stack.c)

add_executable(${CRASH_STACK_BIN} ${CRASH_STACK_SRCS})
target_link_libraries(${CRASH_STACK_BIN})
install(TARGETS ${CRASH_STACK_BIN} DESTINATION libexec)
