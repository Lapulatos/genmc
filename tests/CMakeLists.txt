set(SCRIPT_DIR ${CMAKE_SOURCE_DIR}/scripts)
add_test(
    NAME fast-driver
    COMMAND bash -c "${SCRIPT_DIR}/fast-driver.sh > ${SCRIPT_DIR}/fast-driver.log"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(
    NAME randomize-driver
    COMMAND bash -c "${SCRIPT_DIR}/randomize-driver.sh > ${SCRIPT_DIR}/randomize-driver.log"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(NAME run-parallel
    COMMAND bash -c "${SCRIPT_DIR}/run-parallel.sh > ${SCRIPT_DIR}/run-parallel.log"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(NAME run-relinche-impl
    COMMAND bash -c "${SCRIPT_DIR}/run-relinche-impl.sh > ${SCRIPT_DIR}/run-relinche-impl.log"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(NAME run-relinche-spec
    COMMAND bash -c "${SCRIPT_DIR}/run-relinche-spec.sh > ${SCRIPT_DIR}/run-relinche-spec.log"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_test(
    NAME cli-model-file
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cli/model-file.sh
            $<TARGET_FILE:genmc>
            ${CMAKE_SOURCE_DIR}/models/cat/sc.cat
            ${CMAKE_SOURCE_DIR}/tests/correct/litmus/SB/variants/sb0.c
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(
    NAME cat-sc-differential
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cat/sc-differential.sh
            $<TARGET_FILE:genmc>
            ${CMAKE_SOURCE_DIR}/models/cat/sc.cat
            ${CMAKE_SOURCE_DIR}/tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(
    NAME cat-tso-differential
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cat/tso-differential.sh
            $<TARGET_FILE:genmc>
            ${CMAKE_SOURCE_DIR}/models/cat/tso.cat
            ${CMAKE_SOURCE_DIR}/tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(
    NAME cat-pso-model-proof
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cat/pso-model-proof.sh
            $<TARGET_FILE:genmc>
            ${CMAKE_SOURCE_DIR}/models/cat
            ${CMAKE_CURRENT_SOURCE_DIR}/cat/programs/WW+RR.c
            ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(
    NAME caat-recursive-differential
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cat/recursive-differential.sh
            $<TARGET_FILE:genmc>
            ${CMAKE_SOURCE_DIR}/models/cat
            ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(
    NAME caat-online-mutation-stress
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cat/online-mutation-stress.sh
            $<TARGET_FILE:genmc>
            ${CMAKE_SOURCE_DIR}/models/cat
            ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(
    NAME finite-skeleton-error-replay
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cat/finite-skeleton-error-replay.sh
            $<TARGET_FILE:genmc>
            ${CMAKE_SOURCE_DIR}/models/cat/recursive-sc.cat
            ${CMAKE_CURRENT_SOURCE_DIR}/cat/programs/finite-replay-smoke.ll
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(
    NAME finite-skeleton-strict-admission
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cat/finite-skeleton-strict-admission.sh
            $<TARGET_FILE:genmc>
            ${CMAKE_CURRENT_SOURCE_DIR}/cat/programs/finite-unsupported-switch.ll
            ${CMAKE_CURRENT_SOURCE_DIR}/cat/programs/finite-unsupported-gep.ll
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(
    NAME finite-skeleton-verdict-oracle
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cat/finite-skeleton-verdict-oracle.sh
            $<TARGET_FILE:genmc>
            ${CMAKE_SOURCE_DIR}/models/cat
            ${CMAKE_CURRENT_SOURCE_DIR}/cat/programs/finite-replay-smoke.ll
            ${CMAKE_CURRENT_SOURCE_DIR}/cat/programs/finite-safe-smoke.ll
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(
    NAME sc-rvf-integration
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cat/sc-rvf-integration.sh
            $<TARGET_FILE:genmc>
            ${CMAKE_SOURCE_DIR}/models/cat/recursive-sc.cat
            ${CMAKE_CURRENT_SOURCE_DIR}/correct/litmus/IRIWish/variants/IRIWish0.c
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(
    NAME sc-rvf-outcomes
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cat/sc-rvf-outcomes.sh
            $<TARGET_FILE:genmc>
            ${CMAKE_SOURCE_DIR}/models/cat/recursive-sc.cat
            ${CMAKE_CURRENT_SOURCE_DIR}/cat/programs
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(
    NAME sc-rvf-fallback
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cat/sc-rvf-fallback.sh
            $<TARGET_FILE:genmc>
            ${CMAKE_SOURCE_DIR}/models/cat/recursive-sc.cat
            ${CMAKE_CURRENT_SOURCE_DIR}/wrong/safety/fib_bench/variants/fib_bench0.c
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(
    NAME sc-rvf-mutex-outcomes
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cat/sc-rvf-mutex-outcomes.sh
            $<TARGET_FILE:genmc>
            ${CMAKE_SOURCE_DIR}/models/cat/recursive-sc.cat
            ${CMAKE_CURRENT_SOURCE_DIR}/cat/programs/rvf-mutex-outcome.inc
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(
    NAME sc-rvf-iriwish-outcomes
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cat/sc-rvf-iriwish-outcomes.sh
            $<TARGET_FILE:genmc>
            ${CMAKE_SOURCE_DIR}/models/cat/recursive-sc.cat
            ${CMAKE_CURRENT_SOURCE_DIR}/cat/programs/rvf-iriwish-outcome.inc
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(
    NAME sc-rvf-mutex-iriwish-outcomes
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cat/sc-rvf-iriwish-outcomes.sh
            $<TARGET_FILE:genmc>
            ${CMAKE_SOURCE_DIR}/models/cat/recursive-sc.cat
            ${CMAKE_CURRENT_SOURCE_DIR}/cat/programs/rvf-iriwish-outcome.inc
            RVF_IRIWISH_WITH_MUTEX
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(
    NAME sc-rvf-reduced-outcomes
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cat/sc-rvf-reduced-outcomes.sh
            $<TARGET_FILE:genmc>
            ${CMAKE_SOURCE_DIR}/models/cat/recursive-sc.cat
            ${CMAKE_CURRENT_SOURCE_DIR}/cat/programs
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(
    NAME sc-rvf-assume-outcomes
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cat/sc-rvf-assume-outcomes.sh
            $<TARGET_FILE:genmc>
            ${CMAKE_SOURCE_DIR}/models/cat/recursive-sc.cat
            ${CMAKE_CURRENT_SOURCE_DIR}/cat/programs
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(
    NAME sc-rvf-regional-na-load
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cat/sc-rvf-regional-na-load.sh
            $<TARGET_FILE:genmc>
            ${CMAKE_SOURCE_DIR}/models/cat/recursive-sc.cat
            ${CMAKE_CURRENT_SOURCE_DIR}/cat/programs/rvf-regional-na-load.c
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(
    NAME sc-rvf-regional-loop
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cat/sc-rvf-regional-loop.sh
            $<TARGET_FILE:genmc>
            ${CMAKE_SOURCE_DIR}/models/cat/recursive-sc.cat
            ${CMAKE_CURRENT_SOURCE_DIR}/cat/programs
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
# Unit tests
add_subdirectory(unit)
