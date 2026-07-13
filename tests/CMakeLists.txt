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

# Unit tests
add_subdirectory(unit)
