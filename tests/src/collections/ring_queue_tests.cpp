#include "../../expect.h"
#include "../../test_manager.h"

#include "common/collections/ring_queue.h"

u8 ring_queue_create_and_destroy(void)
{
    Ring_Queue ring_queue;
    ring_queue_create(&ring_queue, sizeof(i32));

    expect_true(ring_queue.data != 0);
    expect_equal(ring_queue.capacity, RING_QUEUE_DEFAULT_CAPACITY);
    expect_equal(ring_queue.stride, sizeof(i32));
    expect_equal(ring_queue.head, 0);
    expect_equal(ring_queue.tail, 0);

    expect_equal(ring_queue_length(&ring_queue), 0);
    expect_true(ring_queue_is_empty(&ring_queue));

    ring_queue_destroy(&ring_queue);
    expect_true(ring_queue.data == 0);
    expect_equal(ring_queue.capacity, 0);
    expect_equal(ring_queue.stride, 0);
    expect_equal(ring_queue.head, 0);
    expect_equal(ring_queue.tail, 0);

    return true;
}

u8 ring_queue_enqueue_and_dequeue(void)
{
    typedef struct {
        i32 foo;
        i64 bar;
    } My_Data;

    Ring_Queue ring_queue;

    u64 initial_capacity = 5;
    ring_queue_reserve(&ring_queue, initial_capacity, sizeof(My_Data));

    My_Data imd1 = { .foo = 1, .bar = 101 };
    My_Data imd2 = { .foo = 2, .bar = 102 };
    My_Data imd3 = { .foo = 3, .bar = 103 };
    My_Data imd4 = { .foo = 4, .bar = 104 };
    My_Data imd5 = { .foo = 5, .bar = 105 };
    My_Data imd6 = { .foo = 6, .bar = 106 };

    expect_true(ring_queue_enqueue(&ring_queue, &imd1));
    expect_equal(ring_queue_length(&ring_queue), 1);

    expect_true(ring_queue_enqueue(&ring_queue, &imd2));
    expect_true(ring_queue_enqueue(&ring_queue, &imd3));
    expect_true(ring_queue_enqueue(&ring_queue, &imd4));
    expect_true(ring_queue_enqueue(&ring_queue, &imd5));

    expect_equal(ring_queue_length(&ring_queue), 5);
    expect_true(ring_queue_is_full(&ring_queue));

    expect_false(ring_queue_enqueue(&ring_queue, &imd6));
    expect_equal(ring_queue_length(&ring_queue), 5);

    My_Data omd1 = {};
    My_Data omd2 = {};
    My_Data omd3 = {};
    My_Data omd4 = {};
    My_Data omd5 = {};
    My_Data omd6 = {};

    expect_true(ring_queue_dequeue(&ring_queue, &omd1));
    expect_equal(omd1.foo, 1);
    expect_equal(omd1.bar, 101);

    expect_true(ring_queue_dequeue(&ring_queue, &omd2));
    expect_equal(omd2.foo, 2);
    expect_equal(omd2.bar, 102);

    expect_true(ring_queue_dequeue(&ring_queue, &omd3));
    expect_equal(omd3.foo, 3);
    expect_equal(omd3.bar, 103);

    expect_equal(ring_queue_length(&ring_queue), 2);

    expect_true(ring_queue_enqueue(&ring_queue, &imd6));

    expect_true(ring_queue_dequeue(&ring_queue, &omd4));
    expect_equal(omd4.foo, 4);
    expect_equal(omd4.bar, 104);

    expect_true(ring_queue_dequeue(&ring_queue, &omd5));
    expect_equal(omd5.foo, 5);
    expect_equal(omd5.bar, 105);

    expect_true(ring_queue_dequeue(&ring_queue, &omd6));
    expect_equal(omd6.foo, 6);
    expect_equal(omd6.bar, 106);

    expect_true(ring_queue_is_empty(&ring_queue));

    ring_queue_destroy(&ring_queue);

    return true;
}

void ring_queue_register_tests(void)
{
    test_manager_register_test(ring_queue_create_and_destroy, "ring queue: create and destroy");
    test_manager_register_test(ring_queue_enqueue_and_dequeue, "ring queue: enqueue and dequeue");
}
