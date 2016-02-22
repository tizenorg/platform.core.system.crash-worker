set(CRASH_PIPE_BIN "crash-pipe")
set(CRASH_PIPE_SRCS crash-pipe.c)

add_executable(${CRASH_PIPE_BIN} ${CRASH_PIPE_SRCS})
install(TARGETS ${CRASH_PIPE_BIN} DESTINATION libexec)
