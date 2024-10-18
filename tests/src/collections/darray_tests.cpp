#include "expect.h"
#include "test_manager.h"

#include "collections/darray.h"

bool darray_create_and_destroy(void)
{
    void *array = darray_create(sizeof(i32));
    expect_true(array != 0);
    expect_equal(darray_capacity(array), DARRAY_DEFAULT_CAPACITY);
    expect_equal(darray_length(array), 0);
    expect_equal(darray_stride(array), sizeof(i32));

    darray_destroy(array);
    expect_equal(((u64 *) array)[DARRAY_FIELD_CAPACITY], 0);
    expect_equal(((u64 *) array)[DARRAY_FIELD_LENGTH], 0);
    expect_equal(((u64 *) array)[DARRAY_FIELD_STRIDE], 0);
    expect_equal(((u64 *) array)[DARRAY_FIELD_COUNT], 0);

    return true;
}

bool darray_push_and_pop(void)
{
    struct mydata {
        i32 foo;
        i64 bar;
    };

    u64 initial_capacity = 3;
    void *array = darray_reserve(initial_capacity, sizeof(struct mydata));

    struct mydata md1 = { 1, 101 };
    struct mydata md2 = { 2, 102 };
    struct mydata md3 = { 3, 103 };
    struct mydata md4 = { 4, 104 };

    darray_push(array, md1);
    darray_push(array, md2);
    darray_push(array, md3);
    expect_equal(darray_capacity(array), initial_capacity);

    darray_push(array, md4);
    expect_equal(darray_capacity(array), initial_capacity * DARRAY_DEFAULT_RESIZE_FACTOR);

    struct mydata omd1 = {};
    struct mydata omd2 = {};
    struct mydata omd3 = {};
    struct mydata omd4 = {};

    darray_pop(array, &omd1);
    darray_pop(array, &omd2);
    darray_pop(array, &omd3);
    darray_pop(array, &omd4);

    expect_equal(omd1.foo, 4);
    expect_equal(omd1.bar, 104);
    expect_equal(omd2.foo, 3);
    expect_equal(omd2.bar, 103);
    expect_equal(omd3.foo, 2);
    expect_equal(omd3.bar, 102);
    expect_equal(omd4.foo, 1);
    expect_equal(omd4.bar, 101);

    expect_equal(darray_length(array), 0);

    darray_destroy(array);

    return true;
}

bool darray_push_at_and_pop_at(void)
{
    i32 *array = (i32 *) darray_create(sizeof(i32));
    darray_push(array, 10);
    darray_push(array, 11);
    darray_push(array, 12);
    darray_push(array, 13);

    darray_push_at(array, 1, 99);

    expect_equal(darray_length(array), 5);
    expect_equal(array[0], 10);
    expect_equal(array[1], 99);
    expect_equal(array[2], 11);
    expect_equal(array[3], 12);
    expect_equal(array[4], 13);

    i32 popped_element;
    darray_pop_at(array, 3, &popped_element);

    expect_equal(popped_element, 12);
    expect_equal(darray_length(array), 4);
    expect_equal(array[0], 10);
    expect_equal(array[1], 99);
    expect_equal(array[2], 11);
    expect_equal(array[3], 13);

    return true;
}

void darray_register_tests(void)
{
    test_manager_register_test(darray_create_and_destroy, "darray: create and destroy");
    test_manager_register_test(darray_push_and_pop, "darray: push and pop");
    test_manager_register_test(darray_push_at_and_pop_at, "darray: push at and pop at");
}
