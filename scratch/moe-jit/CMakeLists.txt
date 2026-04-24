set(AppleClang_MinVersion 11.0.0)
set(Clang_MinVersion 11.0.0)
set(GNU_MinVersion 13.0.0)
set(CMAKE_CXX_STANDARD 23)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-fcoroutines-ts)
    message(NOTICE "mpi-application: using clang, enable corresponding coroutine support")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-fcoroutines)
    message(NOTICE "mpi-application: using gcc, enable corresponding coroutine support")
else ()
    message(SEND_ERROR "mpi-application: unsupported c++ compiler, requires either gcc or clang with c++20 coroutines support")
endif ()

add_library(net-jit-routing
        SHARED
        routing/ipv4-routing-utility.cpp
        routing/hedera-routing.cpp
        routing/hedera-routing-helper.cpp
)

add_executable(
        net-jit
        # main.cpp
        # fattree-test-no-reconfig.cpp
        # dragonfly-test-no-reconfig.cpp
        # fattree-test-with-hedera.cpp
        # dragonfly-test-with-MUSE.cpp
        fattree-test-with-hedera-multiapp.cpp
        utility.cpp
        net-jit-application.h
        topology/fattree-topology.cpp
        topology/dragonfly-topology.cpp
)

add_executable(
        varys-test
        fattree-test-with-varys-multiapp.cpp
        utility.cpp
        net-jit-application.h
        topology/fattree-topology.cpp
        topology/dragonfly-topology.cpp
        varys.h varys.h)

add_executable(
        no-varys-test
        fattree-test-no-varys-multiapp.cpp
        utility.cpp
        net-jit-application.h
        topology/fattree-topology.cpp
        topology/dragonfly-topology.cpp
        varys.h varys.h)




add_executable(
        net-jit-test
        test.cpp
        utility.cpp
        net-jit-application.h
        dml-application.h
)

target_compile_options(
        net-jit
        PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
)

target_link_options(
        net-jit
        PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address>
)

target_link_libraries(
        net-jit
        net-jit-routing
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libmpi-application}
        ${libflow-monitor}
)

target_compile_options(
        net-jit-test
        PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
)

target_link_options(
        net-jit-test
        PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address>
)

target_link_libraries(
        net-jit-test
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libmpi-application}
)


target_compile_options(
        varys-test
        PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
)

target_link_options(
        varys-test
        PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address>
)

target_link_libraries(
        varys-test
        net-jit-routing
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libmpi-application}
        ${libflow-monitor}
)


target_compile_options(
        no-varys-test
        PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
)

target_link_options(
        no-varys-test
        PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address>
)

target_link_libraries(
        no-varys-test
        net-jit-routing
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libmpi-application}
        ${libflow-monitor}
)

add_executable(
        alltoallv-test
        fattree-test-with-all2allv.cpp
        utility.cpp
        net-jit-application.h
        topology/fattree-topology.cpp
        topology/dragonfly-topology.cpp
        varys.h varys.h)

target_compile_options(
        alltoallv-test
        PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
)

target_link_options(
        alltoallv-test
        PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address>
)

target_link_libraries(
        alltoallv-test
        net-jit-routing
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libmpi-application}
        ${libflow-monitor}
)

add_executable(
        simple-alltoallv-test
        simple-alltoallv-test.cpp
        utility.cpp # 包含 retrieveIPAddress 等辅助函数
)

target_link_libraries(
        simple-alltoallv-test
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libmpi-application}
)

add_executable(
        fattree-simple-alltoallv-test
        fattree-simple-alltoallv-test.cpp
        utility.cpp
        topology/fattree-topology.cpp
        topology/dragonfly-topology.cpp
)

target_link_libraries(
        fattree-simple-alltoallv-test
        net-jit-routing
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libmpi-application}
)

add_executable(
        fattree-moe-test
        fattree-moe-test.cpp
        utility.cpp
        topology/fattree-topology.cpp
        topology/dragonfly-topology.cpp
)

target_link_libraries(
        fattree-moe-test
        net-jit-routing
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libmpi-application}
)

add_executable(
        fattree-moe-scheduling-test
        fattree-moe-scheduling-test.cpp
        utility.cpp
        topology/fattree-topology.cpp
        topology/dragonfly-topology.cpp
)

target_link_libraries(
        fattree-moe-scheduling-test
        net-jit-routing
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libmpi-application}
)

add_executable(
        fattree-moe-centralized-test
        fattree-moe-centralized-test.cpp
        utility.cpp
        topology/fattree-topology.cpp
        topology/dragonfly-topology.cpp
)

target_link_libraries(
        fattree-moe-centralized-test
        net-jit-routing
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libmpi-application}
)


add_executable(
        fattree-moe-trace-test
        fattree-moe-trace-test.cpp
        utility.cpp
        topology/fattree-topology.cpp
        topology/dragonfly-topology.cpp
)

target_link_libraries(
        fattree-moe-trace-test
        net-jit-routing
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libmpi-application}
        ${libflow-monitor}
)


add_executable(
        test-moe-parser-basic
        test-moe-parser-basic.cpp
)

add_executable(
        test-moe-parser-scaling
        test-moe-parser-scaling.cpp
)


add_executable(
        dragonfly-moe-trace-test
        dragonfly-moe-trace-test.cpp
        utility.cpp
        topology/fattree-topology.cpp
        topology/dragonfly-topology.cpp
)

target_link_libraries(
        dragonfly-moe-trace-test
        net-jit-routing
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libmpi-application}
        ${libflow-monitor}
        ${libmpi-application}
        ${libflow-monitor}
)

add_executable(
        fattree-moe-fixed-prob-test
        fattree-moe-fixed-prob-test.cpp
        utility.cpp
        topology/fattree-topology.cpp
        topology/dragonfly-topology.cpp
)

target_link_libraries(
        fattree-moe-fixed-prob-test
        net-jit-routing
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libmpi-application}
        ${libflow-monitor}
        ${libmpi-application}
        ${libflow-monitor}
)

add_executable(
        fattree-moe-multijob-test
        fattree-moe-multijob-test.cpp
        utility.cpp
        topology/fattree-topology.cpp
        topology/dragonfly-topology.cpp
)

target_link_libraries(
        fattree-moe-multijob-test
        net-jit-routing
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libmpi-application}
        ${libflow-monitor}
)

add_executable(
        spine-leaf-moe-test
        spine-leaf-moe-test.cpp
        utility.cpp
        topology/spine-leaf-topology.cpp
)

target_link_libraries(
        spine-leaf-moe-test
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libmpi-application}
)

add_executable(
        spine-leaf-trace-test
        spine-leaf-trace-test.cpp
        utility.cpp
        topology/spine-leaf-topology.cpp
)

target_link_libraries(
        spine-leaf-trace-test
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libmpi-application}
)

add_executable(
        spine-leaf-comparison
        spine-leaf-comparison.cpp
        utility.cpp
        topology/spine-leaf-topology.cpp
)

target_link_libraries(
        spine-leaf-comparison
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libmpi-application}
        ${libflow-monitor}
)

add_executable(
        spine-128
        spine-128.cpp
        utility.cpp
        topology/spine-leaf-topology.cpp
)

target_link_libraries(
        spine-128
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libmpi-application}
        ${libflow-monitor}
)


add_executable(
        spine-8app
        spine-8app.cpp
        utility.cpp
        topology/spine-leaf-topology.cpp
)

target_link_libraries(
        spine-8app
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libmpi-application}
        ${libflow-monitor}
)
