/**
 * @file test_sec9_10.c
 * @author Pavol Vican
 * @brief Cmocka test for RFC 6020 section 9.10 conformance.
 *
 * Copyright (c) 2016 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <errno.h>
#include <unistd.h>
#include <cmocka.h>
#include <string.h>
#include <sys/wait.h>

#include "tests/config.h"
#include "libyang.h"

#define TEST_DIR "sec9_10"
#define TEST_NAME test_sec9_10
#define TEST_SCHEMA_COUNT 3
#define TEST_SCHEMA_LOAD_FAIL 1,1,0
#define TEST_DATA_FILE_COUNT 6
#define TEST_DATA_FILE_LOAD_FAIL 1,1,0,1,0,1

struct state {
    struct ly_ctx *ctx;
    struct lyd_node *node;
};

static int
setup_f(void **state)
{
    struct state *st;

    (*state) = st = calloc(1, sizeof *st);
    if (!st) {
        fprintf(stderr, "Memory allocation error");
        return -1;
    }

    /* libyang context */
    st->ctx = ly_ctx_new(TESTS_DIR "/conformance/" TEST_DIR, 0);
    if (!st->ctx) {
        fprintf(stderr, "Failed to create context.\n");
        return -1;
    }

    return 0;
}

static int
teardown_f(void **state)
{
    struct state *st = (*state);

    lyd_free_withsiblings(st->node);
    ly_ctx_destroy(st->ctx, NULL);
    free(st);
    (*state) = NULL;

    return 0;
}

static void
TEST_IDENTITYREF(void **state)
{
    struct state *st = (*state);
    const int schemas_fail[] = {TEST_SCHEMA_LOAD_FAIL};
    const int data_files_fail[] = {TEST_DATA_FILE_LOAD_FAIL};
    char buf[1024];
    LYS_INFORMAT schema_format = LYS_IN_YANG;
    const struct lys_module *mod;
    int i, j, ret;

    for (i = 0; i < 2; ++i) {
        for (j = 0; j < TEST_SCHEMA_COUNT; ++j) {
            sprintf(buf, TESTS_DIR "/conformance/" TEST_DIR "/mod%d.%s", j + 1, (schema_format == LYS_IN_YANG ? "yang" : "yin"));
            mod = lys_parse_path(st->ctx, buf, schema_format);
            if (schemas_fail[j]) {
                assert_ptr_equal(mod, NULL);
            } else {
                assert_ptr_not_equal(mod, NULL);
            }
        }

        for (j = 0; j < TEST_DATA_FILE_COUNT; ++j) {
            sprintf(buf, TESTS_DIR "/conformance/" TEST_DIR "/data%d.xml", j + 1);
            st->node = lyd_parse_path(st->ctx, buf, LYD_XML, LYD_OPT_CONFIG | LYD_OPT_NOAUTODEL);
            if (data_files_fail[j]) {
                assert_ptr_equal(st->node, NULL);
            } else {
                assert_ptr_not_equal(st->node, NULL);
            }
            lyd_free_withsiblings(st->node);
            st->node = NULL;
        }

        if (schema_format == LYS_IN_YANG) {
            /* convert the modules */
            for (j = 0; j < TEST_SCHEMA_COUNT; ++j) {
                sprintf(buf, BUILD_DIR "/yang2yin "
                             TESTS_DIR "/conformance/" TEST_DIR "/mod%d.yang "
                             TESTS_DIR "/conformance/" TEST_DIR "/mod%d.yin", j + 1, j + 1);
                ret = system(buf);
                if (ret == -1) {
                    fprintf(stderr, "system() failed (%s).\n", strerror(errno));
                    fail();
                } else if (WEXITSTATUS(ret) != 0) {
                    fprintf(stderr, "Executing command \"%s\" finished with %d.\n", buf, WEXITSTATUS(ret));
                    fail();
                }
            }

            schema_format = LYS_IN_YIN;
        } else {
            /* remove the modules */
            for (j = 0; j < TEST_SCHEMA_COUNT; ++j) {
                sprintf(buf, TESTS_DIR "/conformance/" TEST_DIR "/mod%d.yin", j + 1);
                if (unlink(buf)) {
                    fprintf(stderr, "unlink() on \"%s\" failed (%s).\n", buf, strerror(errno));
                }
            }
        }
    }
}

/* in data6.xml we have value defined in mod.yang which is just imported in previous test and
 * the data are not valid. Here, mod.yang is loaded as import by mod-dflt.yang, but since it
 * uses identity value from mod.yang as its default, the mod.yang is change to be implemented.
 */
static void
TEST_IDENTITYREF2(void **state)
{
    struct state *st = (*state);
    const struct lys_module *mod;
    const char *middle_data = "<test xmlns=\"urn:cesnet:mod3\" xmlns:mm=\"urn:cesnet:mod-middle\">mm:j4</test>";

    /* mod is imported */
    mod = lys_parse_path(st->ctx, TESTS_DIR "/conformance/" TEST_DIR "/mod3.yang", LYS_IN_YANG);
    assert_ptr_not_equal(mod, NULL);

    /* invalid identityref value from not-implemented schema */
    st->node = lyd_parse_path(st->ctx, TESTS_DIR "/conformance/" TEST_DIR "/data6.xml", LYD_XML, LYD_OPT_CONFIG);
    assert_ptr_equal(st->node, NULL);

    mod = lys_parse_path(st->ctx, TESTS_DIR "/conformance/" TEST_DIR "/mod-dflt-invalid.yang", LYS_IN_YANG);
    assert_ptr_equal(mod, NULL);

    /* mod is set to be implemented */
    mod = lys_parse_path(st->ctx, TESTS_DIR "/conformance/" TEST_DIR "/mod-dflt.yang", LYS_IN_YANG);
    assert_ptr_not_equal(mod, NULL);

    /* mod is implemented so the identityref value is valid here */
    st->node = lyd_parse_path(st->ctx, TESTS_DIR "/conformance/" TEST_DIR "/data6.xml", LYD_XML, LYD_OPT_CONFIG);
    assert_ptr_not_equal(st->node, NULL);
    lyd_free_withsiblings(st->node);

    /* but mod-middle is still not implemented, so mod-middle:j1 value is invalid */
    st->node = lyd_parse_mem(st->ctx, middle_data, LYD_XML, LYD_OPT_CONFIG);
    assert_ptr_equal(st->node, NULL);

    /* but making it implemented the data can be loaded */
    assert_int_equal(lys_set_implemented(ly_ctx_get_module(st->ctx, "mod-middle", NULL, 0)), 0);
    st->node = lyd_parse_mem(st->ctx, middle_data, LYD_XML, LYD_OPT_CONFIG);
    assert_ptr_not_equal(st->node, NULL);
}

int
main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(TEST_IDENTITYREF, setup_f, teardown_f),
        cmocka_unit_test_setup_teardown(TEST_IDENTITYREF2, setup_f, teardown_f),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
