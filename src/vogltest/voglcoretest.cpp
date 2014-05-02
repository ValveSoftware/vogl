/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/

// File: voglcoretest.cpp
#include "vogl_core.h"
#include "vogl_colorized_console.h"
#include "vogl_command_line_params.h"

#include "vogl_regex.h"
#include "vogl_bigint128.h"
#include "vogl_sparse_vector.h"
#include "vogl_sort.h"
#include "vogl_hash_map.h"
#include "vogl_map.h"
#include "vogl_md5.h"
#include "vogl_rh_hash_map.h"

//$ TODO?
//#include "vogl_timer.h"

// map_perf_test(200);
// map_perf_test(2000);
// map_perf_test(20000);
// map_perf_test(200000);
// map_perf_test(2000000);

using namespace vogl;

// These test functions take parameters.
static bool sparse_vectorx_test() { return sparse_vector_test(); }
static bool bigint128x_test() { return bigint128_test(); }

struct test_data_t
{
    const char *name;
    bool (*pfunc1)(void);
    bool (*pfunc2)(uint);
};
static const struct test_data_t g_tests[] =
{
#define DEFTEST(_x) { #_x, _x ## _test, NULL }
#define DEFTEST2(_x) { #_x, NULL, _x ## _test }
    DEFTEST(rh_hash_map),
    DEFTEST(object_pool),
    DEFTEST(dynamic_string),
    DEFTEST(md5),
    DEFTEST(introsort),
    DEFTEST(rand),
    DEFTEST(regexp),
    DEFTEST(strutils),
    DEFTEST(map),
    DEFTEST(hash_map),
    DEFTEST(sort),
    DEFTEST2(sparse_vector),
    DEFTEST2(bigint128),
#undef DEFTEST
#undef DEFTEST2
};

//----------------------------------------------------------------------------------------------------------------------
// command line params
//----------------------------------------------------------------------------------------------------------------------
static command_line_param_desc g_command_line_param_descs[] =
{
    { "test", 0, false, "Run named list of tests." },
    { "all", 0, false, "Run all tests." },

    { "help", 0, false, "Display this help" },
    { "?", 0, false, "Display this help" },
};

//----------------------------------------------------------------------------------------------------------------------
// tool_print_help
//----------------------------------------------------------------------------------------------------------------------
static void tool_print_help()
{
    console::printf("Usage: vogltest [ --all ] [--test testname1 testname2 ...]\n");
    console::printf("\nCommand line options:\n");

    dump_command_line_info(VOGL_ARRAY_SIZE(g_command_line_param_descs), g_command_line_param_descs, "--");
    
    printf("\nVoglcore tests:\n");

    for (size_t i = 0; i < VOGL_ARRAY_SIZE(g_tests); i++)
    {
        printf("  % 2d: %s\n", (int)i, g_tests[i].name);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// init_command_line_params
//----------------------------------------------------------------------------------------------------------------------
static bool init_command_line_params(int argc, char *argv[])
{
    command_line_params::parse_config parse_cfg;
    parse_cfg.m_single_minus_params = true;
    parse_cfg.m_double_minus_params = true;

    if (!g_command_line_params().parse(get_command_line_params(argc, argv), VOGL_ARRAY_SIZE(g_command_line_param_descs), g_command_line_param_descs, parse_cfg))
    {
        console::error("%s: Failed parsing command line parameters!\n", VOGL_FUNCTION_NAME);
        return false;
    }

    if (g_command_line_params().get_value_as_bool("help") || g_command_line_params().get_value_as_bool("?"))
    {
        tool_print_help();
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// run_voglcore_tests
//----------------------------------------------------------------------------------------------------------------------
int run_voglcore_tests(int argc, char *argv[])
{
    colorized_console::init();
    colorized_console::set_exception_callback();

    if (!init_command_line_params(argc, argv))
        return EXIT_FAILURE;

    int num_failures = 0;
    bool arg_test = g_command_line_params().has_key("test");
    bool arg_all = g_command_line_params().has_key("all");

    if (!arg_test && !arg_all)
    {
        tool_print_help();
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < VOGL_ARRAY_SIZE(g_tests); i++)
    {
        // If --all was passed or the name of this test, then run it.
        bool run_test = arg_all || check_for_command_line_param(g_tests[i].name);

        if (run_test)
        {
            printf("\n%" PRIu64 ": %s...\n", i, g_tests[i].name);

            timer tm;
            tm.start();
            int ret = g_tests[i].pfunc1() ? g_tests[i].pfunc1() : g_tests[i].pfunc2(15);
            double time = tm.get_elapsed_secs();

            printf("%" PRIu64 ": %s: %s (%.3f secs)\n", i, g_tests[i].name,
                   (ret == EXIT_SUCCESS) ? "Success" : "Failed", time);

            num_failures += (ret == EXIT_FAILURE);
        }
    }

    if (num_failures == 0)
        printf("All tests succeeded\n");
    else
        fprintf(stderr, "**** %u test(s) failed!\n", num_failures);

    return num_failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
