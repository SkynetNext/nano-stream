# Disruptor Java examples (ported 1:1 from reference/disruptor/src/examples/java)

set(DISRUPTOR_EXAMPLES_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/disruptor_examples)

function(add_disruptor_example target source)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE disruptor-cpp)
  target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/include ${DISRUPTOR_EXAMPLES_ROOT})
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE -Wno-unused-parameter)
  elseif(MSVC)
    target_compile_options(${target} PRIVATE /wd4100)
  endif()
endfunction()

# Each executable corresponds to a Java example with a runnable entry point.
add_disruptor_example(disruptor_example_WaitForShutdown           ${DISRUPTOR_EXAMPLES_ROOT}/WaitForShutdown.cpp)
add_disruptor_example(disruptor_example_WaitForProcessing         ${DISRUPTOR_EXAMPLES_ROOT}/WaitForProcessing.cpp)
add_disruptor_example(disruptor_example_ThreeToOneDisruptor       ${DISRUPTOR_EXAMPLES_ROOT}/ThreeToOneDisruptor.cpp)
add_disruptor_example(disruptor_example_ShutdownOnError           ${DISRUPTOR_EXAMPLES_ROOT}/ShutdownOnError.cpp)
add_disruptor_example(disruptor_example_SequentialThreeConsumers  ${DISRUPTOR_EXAMPLES_ROOT}/SequentialThreeConsumers.cpp)
add_disruptor_example(disruptor_example_PullWithPoller            ${DISRUPTOR_EXAMPLES_ROOT}/PullWithPoller.cpp)
add_disruptor_example(disruptor_example_PullWithBatchedPoller     ${DISRUPTOR_EXAMPLES_ROOT}/PullWithBatchedPoller.cpp)
add_disruptor_example(disruptor_example_Pipeliner                ${DISRUPTOR_EXAMPLES_ROOT}/Pipeliner.cpp)
add_disruptor_example(disruptor_example_DynamicallyAddHandler     ${DISRUPTOR_EXAMPLES_ROOT}/DynamicallyAddHandler.cpp)
add_disruptor_example(disruptor_example_KeyedBatching             ${DISRUPTOR_EXAMPLES_ROOT}/KeyedBatching.cpp)
add_disruptor_example(disruptor_example_MultiProducerWithTranslator ${DISRUPTOR_EXAMPLES_ROOT}/MultiProducerWithTranslator.cpp)
add_disruptor_example(disruptor_example_HandleExceptionOnTranslate ${DISRUPTOR_EXAMPLES_ROOT}/HandleExceptionOnTranslate.cpp)
add_disruptor_example(disruptor_example_EarlyReleaseHandler       ${DISRUPTOR_EXAMPLES_ROOT}/EarlyReleaseHandler.cpp)

# longevent variants
add_disruptor_example(disruptor_example_LongEventMain_lambdas     ${DISRUPTOR_EXAMPLES_ROOT}/longevent/lambdas/LongEventMain.cpp)
add_disruptor_example(disruptor_example_LongEventMain_legacy      ${DISRUPTOR_EXAMPLES_ROOT}/longevent/legacy/LongEventMain.cpp)
add_disruptor_example(disruptor_example_LongEventMain_methodrefs  ${DISRUPTOR_EXAMPLES_ROOT}/longevent/methodrefs/LongEventMain.cpp)

# objectevent main
add_disruptor_example(disruptor_example_ObjectEventMain           ${DISRUPTOR_EXAMPLES_ROOT}/objectevent/Main.cpp)
