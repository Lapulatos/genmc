set(LIB_SOURCES
  genmc/ADT/AdaptiveView.cpp
  genmc/ADT/DepView.cpp
  genmc/ADT/SmallVector.cpp
  genmc/ADT/VectorClock.cpp
  genmc/ADT/View.cpp
  genmc/CAT/Evaluator.cpp
  genmc/CAT/Analysis.cpp
  genmc/CAT/CaatEvaluator.cpp
  genmc/CAT/ConflictCore.cpp
  genmc/CAT/Frontend.cpp
  genmc/CAT/GraphAdapter.cpp
  genmc/CAT/GraphSynchronizer.cpp
  genmc/CAT/IncrementalEvaluator.cpp
  genmc/CAT/LazyCycle.cpp
  genmc/CAT/Model.cpp
  genmc/CAT/Normalized.cpp
  genmc/CAT/Reasoner.cpp
  genmc/CAT/StableGraphAdapter.cpp
  genmc/CAT/Value.cpp
  genmc/Execution/DepExecutionGraph.cpp
  genmc/Execution/EventLabel.cpp
  genmc/Execution/ExecutionGraph.cpp
  genmc/Execution/GraphUtils.cpp
  genmc/Execution/ExecutionState.cpp
  genmc/Execution/Consistency/BoundDecider.cpp
  genmc/Execution/Consistency/CATChecker.cpp
  genmc/Execution/Consistency/ConsistencyChecker.cpp
  genmc/Execution/Consistency/ContextBoundDecider.cpp
  genmc/Execution/Consistency/IMMChecker.cpp
  genmc/Execution/Consistency/RAChecker.cpp
  genmc/Execution/Consistency/RC11Checker.cpp
  genmc/Execution/Consistency/RoundBoundDecider.cpp
  genmc/Execution/Consistency/SCChecker.cpp
  genmc/Execution/Consistency/SymmetryChecker.cpp
  genmc/Execution/Consistency/TSOChecker.cpp
  genmc/Support/Error.cpp
  genmc/Support/NameInfo.cpp
  genmc/Support/Parser.cpp
  genmc/Support/SAddrAllocator.cpp
  genmc/Support/SVal.cpp
  genmc/Support/ThreadPinner.cpp
  genmc/Verification/ChoiceMap.cpp
	genmc/Verification/CATDecisionState.cpp
  genmc/Verification/Config.cpp
  genmc/Verification/FiniteSymbolicSolver.cpp
  genmc/Verification/FiniteSkeletonEncoder.cpp
  genmc/Verification/FiniteSkeletonCAT.cpp
  genmc/Verification/FiniteSkeletonSCCompletion.cpp
	genmc/Verification/GenMCDriver.cpp
	genmc/Verification/RegionalRvfTransaction.cpp
  genmc/Verification/Relinche/LinearizabilityChecker.cpp
  genmc/Verification/Relinche/Linearization.cpp
  genmc/Verification/Relinche/Observation.cpp
  genmc/Verification/Relinche/Specification.cpp
  genmc/Verification/Scheduler.cpp
  genmc/Verification/SCGoodWritesSolver.cpp
  genmc/Verification/SCExecutionGraphAdapter.cpp
  genmc/Verification/SCReadsValueFromState.cpp
)

### GenMC library
find_package(Threads REQUIRED)
find_library(HWLOC hwloc)
find_path(Z3_INCLUDE_DIR z3++.h)
find_library(Z3_LIBRARY z3)

add_library(genmc_lib ${GENMC_LIB_TYPE} ${LIB_SOURCES})
install(TARGETS genmc_lib ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/genmc/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.def")

target_include_directories(genmc_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(genmc_lib PUBLIC Threads::Threads genmc_config_includes)
if(Z3_INCLUDE_DIR AND Z3_LIBRARY)
  target_include_directories(genmc_lib PRIVATE ${Z3_INCLUDE_DIR})
  target_link_libraries(genmc_lib PRIVATE ${Z3_LIBRARY})
  target_compile_definitions(genmc_lib PRIVATE GENMC_HAVE_Z3=1)
  message(STATUS "Finite symbolic solver: Z3 enabled")
else()
  target_compile_definitions(genmc_lib PRIVATE GENMC_HAVE_Z3=0)
  message(STATUS "Finite symbolic solver: Z3 unavailable (lane will fail open)")
endif()
if(HWLOC)
  target_link_libraries(genmc_lib PRIVATE hwloc)
else()
  message(STATUS "hwloc not found. Thread pinning disabled.")
endif()
