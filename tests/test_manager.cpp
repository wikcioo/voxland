#include "test_manager.h"

#include <stdio.h>

#include "log.h"
#include "collections/darray.h"

typedef struct {
    pfn_test test_function;
    const char *description;
} test_instance_t;

LOCAL test_instance_t *test_instances;

void test_manager_init(void)
{
    test_instances = (test_instance_t *) darray_create(sizeof(test_instance_t));
}

void test_manager_register_test(pfn_test test, const char *description)
{
    test_instance_t inst = { test, description };
    darray_push(test_instances, inst);
}

void test_manager_run_all_tests(void)
{
    u32 passed = 0;
    u32 failed = 0;
    u32 skipped = 0;

    u64 length = darray_length(test_instances);
    for (u32 i = 0; i < length; i++) {
        printf("[test] %s... ", test_instances[i].description);
        fflush(stdout);
        u8 result = test_instances[i].test_function();
        if (result == true) {
            printf(INFO_COLOR " passed\n" RESET_COLOR);
            passed++;
        } else if (result == false) {
            // the failing test prints out an error message
            failed++;
        } else if (result == SKIP_RESULT) {
            printf(WARN_COLOR " skipped\n" RESET_COLOR);
            skipped++;
        }
    }

    char results_buf[256] = {};
    snprintf(results_buf, sizeof(results_buf),
             "%ssummary%s: finished running %llu tests: %s%u passed%s, %s%u failed%s, %s%u skipped%s\n",
             DEBUG_COLOR, RESET_COLOR, length,
             (passed > 0 ? INFO_COLOR : ""), passed, RESET_COLOR,
             (failed > 0 ? ERROR_COLOR : ""), failed, RESET_COLOR,
             (skipped > 0 ? WARN_COLOR : ""), skipped, RESET_COLOR);
    printf("%s", results_buf);
}

void test_manager_shutdown(void)
{
    darray_destroy(test_instances);
}
