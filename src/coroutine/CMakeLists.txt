set(AppleClang_MinVersion 11.0.0)
set(Clang_MinVersion 11.0.0)
set(GNU_MinVersion 10.0.0)
set(CMAKE_CXX_STANDARD 20)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-fcoroutines-ts)
    message(NOTICE "coroutine: using clang, enable corresponding coroutine support")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-fcoroutines)
    message(NOTICE "coroutine: using gcc, enable corresponding coroutine support")
else ()
    message(SEND_ERROR "coroutine: unsupported c++ compiler, requires either gcc or clang with c++20 coroutines support")
endif ()


build_lib(
        LIBNAME coroutine
        IGNORE_PCH
        SOURCE_FILES
        model/coroutine-socket.cpp
        HEADER_FILES
        model/operation.h
        model/awaitable.h
        model/operation-trait.h
        model/operation-type.h
        model/coroutine-socket.h
        LIBRARIES_TO_LINK
            ${libcore}
            ${libnetwork}
            ${libinternet}
)