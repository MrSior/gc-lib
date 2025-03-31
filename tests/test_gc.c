#include "gc/gc.h"
#include "gc/minunit.h"

char* first_test() {
    int a = 1;
    int b = 1;
    MU_ASSERT(a + b == 2, "1 + 1 != 1");
    return NULL;
}

char* second_test() {
    int a = 1;
    int b = 1;
    MU_ASSERT(a + b == 2, "1 + 1 != 3");
    return NULL;
}

int tests_run = 0;

static char* test_suite()
{
    printf("=====[ GC tests ]=====\n");
    MU_RUN_TEST(first_test);
    MU_RUN_TEST(second_test);
    return 0;
}

int main()
{
    char *result = test_suite();
    if (result) {
        printf("%s\n", result);
    } else {
        printf("ALL TESTS PASSED\n");
    }
    printf("Tests run: %d\n", tests_run);
    printf("======================\n");
    return result != 0;
}