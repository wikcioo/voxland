#include "test_manager.h"

#include "src/collections/darray_tests.h"
#include "src/memory/arena_allocator_tests.h"

int main(void)
{
    test_manager_init();

    darray_register_tests();
    arena_allocator_register_tests();

    test_manager_run_all_tests();
    test_manager_shutdown();

    return 0;
}
