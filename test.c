#include <stdio.h>
#include <check.h>
#include <stdlib.h>
#include "qthread.h"

int flag1,flag2,flag3;
void *test1_fn(void *arg)
{
    	flag1 = 1;
    	qthread_exit("2");
}
START_TEST(test_queue_functions)
{
	struct threadq q = {.head = NULL, .tail = NULL};
	struct qthread t1 = {.next = (void*)1, .exited_flag = 1};
	struct qthread t2 = {.next = (void*)2, .exited_flag = 2};
	struct qthread t3 = {.next = (void*)3, .exited_flag = 3};
	struct qthread t4 = {.next = (void*)4, .exited_flag = 4};

	ck_assert(tq_empty(&q));    // q=()
    	tq_append(&q, &t1);         // q=(1)
    	ck_assert(!tq_empty(&q));   // q=(1)

    	tq_append(&q, &t3);         // q=(1, 3)
    	tq_append(&q, &t2);         // q=(1, 3, 2)
    	struct qthread* t = tq_pop(&q); // t=1, q=(3, 2)
    	ck_assert_int_eq(t->exited_flag, 1);

    	tq_append(&q, t);           // q=(3, 2, 1)
    	t = tq_pop(&q);        // t=3, q=(2, 1)
    	ck_assert_int_eq(t->exited_flag, 3);

    	t = tq_pop(&q);             // t=2, q=(1)
    	ck_assert_int_eq(t->exited_flag, 2);

    	t = tq_pop(&q);             // t=1, q=()
    	ck_assert_int_eq(t->exited_flag, 1);
    	ck_assert(tq_empty(&q));
    	t = tq_pop(&q);             // t=
    	ck_assert_ptr_null(t);
}END_TEST

START_TEST(test_join_with_yield)
{
    	qthread_t th = qthread_create(test1_fn, "1");
    	ck_assert_int_eq(flag1, 0);
    	qthread_init();
    	qthread_yield();
    	ck_assert_int_eq(flag1, 1);
    	char *val = qthread_join(th);
    	ck_assert_str_eq(val, "2");

}END_TEST

START_TEST(test_join_without_yield)
{
    	qthread_t th = qthread_create(test1_fn, "1");
    	ck_assert_int_eq(flag2, 0);
    	char *val = qthread_join(th);
    	ck_assert_int_eq(flag2, 1);

}END_TEST

void *test2_fn(void *arg)
{
    	flag3 = 1;
    	return "5";
}
START_TEST(return_val_from_function)
{
    qthread_t th = qthread_create(test2_fn, "1");
    ck_assert_int_eq(flag3, 0);
    qthread_init();
    char *val = qthread_join(th);
    ck_assert_str_eq(val, "5"); //asserting whether return value is the same
}END_TEST

int flag4;
void *test3_fn(void *arg)
{
    	flag4 = 5;
    	qthread_exit("2");
}
START_TEST(two_threads_active)
{
    qthread_t th = qthread_create(test3_fn, "1"); //creating thread1
    ck_assert_int_eq(flag4, 0);
    qthread_t th1 = qthread_create(test3_fn, "1"); //creating thread2
    ck_assert_int_eq(flag4, 0);
    qthread_init();
    char *val = qthread_join(th);
    ck_assert_int_eq(flag4, 5);
    ck_assert_str_eq(val, "2");
    char *val1 = qthread_join(th1);//checking flag =4 for both threads (to see if they run)
    ck_assert_int_eq(flag4, 5);
    ck_assert_str_eq(val1, "2");
}END_TEST

qthread_mutex_t *m1;
int flagMu;
void *thread_fn(void *arg)
{
    flagMu = 1;
    qthread_mutex_lock(m1);
    flagMu = 2;
    qthread_mutex_unlock(m1);
    return "3";
}

void *thread_fn2(void *arg)
{
    flagMu = 4;
    qthread_mutex_lock(m1);
    flagMu = 5;
    qthread_mutex_unlock(m1);
    return "6";
}

START_TEST(mutex_one_thread_lock_unlock)
{
    m1 = qthread_mutex_create();
    qthread_t th = qthread_create(thread_fn, NULL);
    qthread_init();
    qthread_mutex_lock(m1);
    qthread_yield();
    ck_assert_int_eq(flagMu, 1);
    qthread_mutex_unlock(m1);
    qthread_yield();
	ck_assert_int_eq(flagMu, 2);
    char *val = qthread_join(th);
    ck_assert_str_eq(val, "3");
}END_TEST

START_TEST(mutex_two_thread_lock_unlock)
{
	m1 = qthread_mutex_create();
	qthread_t th = qthread_create(thread_fn, NULL);
	qthread_t th2 = qthread_create(thread_fn2, NULL);
    qthread_mutex_lock(m1);
	qthread_init();
    qthread_yield();
    ck_assert_int_eq(flagMu, 4);
    qthread_mutex_unlock(m1);
    qthread_yield();
    ck_assert_int_eq(flagMu, 2);
    qthread_yield();
	ck_assert_int_eq(flagMu, 5);
    char *val = qthread_join(th);
	ck_assert_str_eq(val, "3");
    char *val2 = qthread_join(th2);
	ck_assert_str_eq(val2, "6");
}END_TEST

qthread_cond_t *c1;
int flagCond;
void *thread_fn_cond(void *arg)
{
    flagCond = 1;
    qthread_cond_wait(c1, m1);
    flagCond = 2;
    return "3";
}

START_TEST(test_cond_wait_signal)
{
	c1 = qthread_cond_create();
	m1 = qthread_mutex_create();
	qthread_t th = qthread_create(thread_fn_cond, NULL);
	qthread_init();
    qthread_yield();
    ck_assert_int_eq(flagCond, 1);
    qthread_cond_signal(c1);
    qthread_yield();
    ck_assert_int_eq(flagCond, 2);
    char *val = qthread_join(th);
	ck_assert_str_eq(val, "3");
}END_TEST

Suite *create_suite(void)
{
	Suite *s = suite_create("Homework 2");
	TCase *tc = tcase_create("First test set");
	tcase_add_test(tc,test_queue_functions);
	tcase_add_test(tc,test_join_with_yield);
	tcase_add_test(tc,test_join_without_yield);
	tcase_add_test(tc,return_val_from_function);
	tcase_add_test(tc,two_threads_active);
	tcase_add_test(tc,mutex_one_thread_lock_unlock);
	tcase_add_test(tc,mutex_two_thread_lock_unlock);
	tcase_add_test(tc,test_cond_wait_signal);
	suite_add_tcase(s,tc);

	return s;
}

int main()
{
	int n_failed;
	Suite *s;
	SRunner *sr;
	s = create_suite();
	sr = srunner_create(s);
	srunner_run_all(sr,CK_VERBOSE);
	n_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	printf("Failed cases %d",n_failed);
	return 0;
}

