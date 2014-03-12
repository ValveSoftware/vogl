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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <argp.h> 
#include <sys/utsname.h>
#include <string>

#define F_VERBOSE         0x00000001
#define F_I386            0x00000002
#define F_AMD64           0x00000004
#define F_USEMAKE         0x00000008
#define F_CLEAN           0x00000010
#define F_RELEASE         0x00000020
#define F_DEBUG           0x00000040
#define F_CLANG33         0x00000100
#define F_CLANG34         0x00000200
#define F_GCC48           0x00000400
#define F_GCC             0x00000800
#define F_CLEANONLY       0x00001000

static bool g_dryrun = false;

struct arguments_t
{
    unsigned int flags;
    std::string defs;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    arguments_t *arguments = (arguments_t *)state->input;

    switch (key)
    {
    case '?': 
        argp_state_help(state, stderr, ARGP_HELP_LONG | ARGP_HELP_EXIT_OK);
        break;
    case -1:
        {
            const char *argval = &state->argv[state->next - 1][2];
            for (size_t i = 0; ; i++)
            {
                const char *name = state->root_argp->options[i].name;
                if (!name)
                    break;
                if (!strcmp(name, argval))
                {
                    arguments->defs += " -D";
                    arguments->defs += &state->argv[state->next - 1][2];
                    arguments->defs += "=On";
                    return 0;
                }
            }
            printf("Unrecognized option: '%s'\n\n", argval);
            argp_state_help(state, stderr, ARGP_HELP_LONG | ARGP_HELP_EXIT_OK);
        }
        break;
    case 'v': arguments->flags |= F_VERBOSE; break;
    case '3': arguments->flags |= F_I386; break;
    case '6': arguments->flags |= F_AMD64; break;
    case 'm': arguments->flags |= F_USEMAKE; break;
    case 'c': arguments->flags |= F_CLEAN; break;
    case 'o': arguments->flags |= F_CLEANONLY; break;
    case 'd': arguments->flags |= F_DEBUG; break;
    case 'r': arguments->flags |= F_RELEASE; break;
    case 'g': arguments->flags |= F_GCC; break;
    case '8': arguments->flags |= F_GCC48; break;
    case 'l': arguments->flags |= F_CLANG33; break;
    case 'n': arguments->flags |= F_CLANG34; break;
    case 'y': g_dryrun = true; break;
        break;
    }
    return 0; 
}

void errorf(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    exit(-1);
}

void systemf(const char *format, ...)
{
    char command[PATH_MAX];

    va_list args;
    va_start(args, format);
    vsnprintf(command, sizeof(command), format, args);

    printf("  %s\n", command);
    if (!g_dryrun)
        system(command); 

    va_end(args);
}

int main(int argc, char *argv[])
{
    static struct argp_option options[] =
    {
        { "i386",                       '3', 0, 0, "Build 32-bit", 0 },
        { "amd64",                      '6', 0, 0, "Build 64-bit.", 0 },

        { "release",                    'r', 0, 0, "Build release (default).", 1 },
        { "debug",                      'd', 0, 0, "Build debug.", 1 },

        { "clean",                      'c', 0, 0, "Clean (remove directory before build).", 2 },
        { "cleanonly",                  'o', 0, 0, "Clean only (remove directory and exit).", 2 },
        { "usemake",                    'm', 0, 0, "Use make (not Ninja).", 2 },
        { "dry-run",                    'y', 0, 0, "Don't execute commands.", 2 },
        { "verbose",                    'v', 0, 0, "Produce verbose output", 2 },

        { "gcc46",                      'g', 0, 0, "Use gcc 4.6 to build.", 3 },
        { "gcc48",                      '8', 0, 0, "Use gcc 4.8 to build.", 3 },
        { "clang33",                    'l', 0, 0, "Use clang 3.3 to build.", 3 },
        { "clang34",                    'n', 0, 0, "Use clang 3.4 to build.", 3 },

        { "CLANG_ANALYZE",              -1,  0, 0, "Do clang analyze build (will not link).", 100 },
        { "VOGLTEST_LOAD_LIBVOGLTRACE",   -1,  0, 0, "Don't implicitly link against libgl.so", 100 },
        { "VOGLTRACE_NO_PUBLIC_EXPORTS", -1,  0, 0, "Don't define public GL exports in libvogltrace.so.", 100 },
        { "VOGL_ENABLE_ASSERTS",      -1,  0, 0, "Enable assertions in voglcore builds (including release).", 100 },
        { "CLANG_EVERYTHING",           -1,  0, 0, "Do clang build with -Weverything.", 100 },
        { "USE_TELEMETRY",              -1,  0, 0, "Build with Telemetry.", 100 },
        { "WITH_ASAN",                  -1,  0, 0, "Build with Clang address sanitizer.", 100 },
        { "CMAKE_VERBOSE",              -1,  0, 0, "Do verbose cmake.", 100 },

        { "help",                       '?', 0, 0, "Give this help message.", -1 },
        { 0 }
    };

    const char *vogl_proj_dir = getenv("VOGL_PROJ_DIR");
    if (!vogl_proj_dir)
    {
        printf("ERROR: VOGL_PROJ_DIR envvar not set.\n");
        return -1;
    }

    arguments_t args;
    args.flags = 0;
    struct argp argp = { options, parse_opt, 0, "vogl project builder." };

    // Argp moves our argvs around (jerks) - save original order here so we
    //  can launch cmake directly down below if needed.
    std::string cmdline_orig;
    for(int i = 1; i < argc; i++)
    {
        bool quote_arg = !!strchr(argv[i], ' ');
        if (i > 1)
            cmdline_orig += " ";
        if (quote_arg)
            cmdline_orig += "\"";
        cmdline_orig += argv[i];
        if (quote_arg)
            cmdline_orig += "\"";
    }

    // Parse args.
    argp_parse(&argp, argc, argv, ARGP_NO_HELP, 0, &args);

    // Check if we are running under a chroot.
    const char *schroot_user = getenv("SCHROOT_USER");
    if (schroot_user)
    {
        struct utsname uts;
        if(uname(&uts))
            errorf("ERROR: uname failed.\n");
        unsigned int plat_flag = !strcmp(uts.machine, "x86_64") ? F_AMD64 : F_I386;
        args.flags |= plat_flag;
        if ((args.flags & (F_AMD64 | F_I386)) != plat_flag)
            errorf("ERROR: Can't build this arch in this %s chroot.\n", uts.machine);
    }

    if (!(args.flags & (F_I386 | F_AMD64)))
        errorf("ERROR: Need to specify --i386 or --amd64\n");

    // Default to release if not set.
    if (!(args.flags & (F_RELEASE | F_DEBUG)))
        args.flags |= F_RELEASE;

    // Use Ninja if set.
    if (!(args.flags & F_USEMAKE))
        args.defs += " -G Ninja";

    // Get the vogl src directory and add it to our defines.
    args.defs += " ";
    args.defs += vogl_proj_dir;
    args.defs += "/src";

    unsigned int platform = args.flags & (F_I386 | F_AMD64);
    while (platform)
    {
        static const char *libarch[] = { "i386", "x86_64" };
        static const char *suffix[] = { "32", "64" };
        static const char *dirname = "bin";
        static const char *schroot_name[] = { "vogl_precise_i386", "vogl_precise_amd64" };
        static const char *deflibarch[] = { " -DCMAKE_LIBRARY_ARCHITECTURE=i386-linux-gnu", " -DCMAKE_LIBRARY_ARCHITECTURE=x86_64-linux-gnu" };
        int platind = !(platform & F_I386);

        platform &= (platform - 1);

        unsigned int flavor = args.flags & (F_RELEASE | F_DEBUG);
        while (flavor)
        {
            static const char *name[] = { "Release32", "Debug32", "Release64", "Debug64" };
            static const char *build_type[] = { " -DCMAKE_BUILD_TYPE=Release", " -DCMAKE_BUILD_TYPE=Debug" };
            int flavind = !(flavor & F_RELEASE);

            flavor &= (flavor - 1);

            const char *flavor_str = name[2*platind + flavind];
            printf("\033[0;93mBuilding %s - %s\033[0m\n", libarch[platind], flavor_str);

            std::string defines = args.defs;
            defines += build_type[flavind];

            // We need to add this for Ninja builds in the chroots. (Bug in cmake+Ninja)
            if (!(args.flags & F_USEMAKE))
                defines += deflibarch[platind];

            printf("\033[0;93m %s\033[0m\n", defines.c_str());

            // Get our build directory.
            char build_dir[PATH_MAX];
            snprintf(build_dir, sizeof(build_dir), "%s/vogl_build/%s/%s", vogl_proj_dir, dirname, flavor_str);

            // If we are building clean, remove all the files in our build directory.
            if (args.flags & (F_CLEAN | F_CLEANONLY) )
            {

                static const char *subdirs[] = { ".", "OGLSamples_Truc", "OGLSuperBible" };
                for (size_t i = 0; i < sizeof(subdirs) / sizeof(subdirs[0]); i++)
                {
                    systemf("exec rm -f %s/vogl_build/%s/%s/*%s 2> /dev/null", vogl_proj_dir, dirname, subdirs[i], suffix[platind]);
                    systemf("exec rm -f %s/vogl_build/%s/%s/*%s.so 2> /dev/null", vogl_proj_dir, dirname, subdirs[i], suffix[platind]);
                }
                systemf("exec rm -rf %s/vogl_build/%s/%s/*", vogl_proj_dir, dirname, flavor_str);

                if (args.flags & F_CLEANONLY)
                    return 0;
            }

            // Make sure our build dir exists.
            systemf("exec mkdir -p %s", build_dir);

            if (!g_dryrun)
            {
                // Cd to build directory.
                int ret = chdir(build_dir);
                if (ret != 0)
                    errorf("ERROR: chdir(%s) failed: %d\n", build_dir, ret);
            }

            // Check if we are in a schroot. If so, get commands to switch to chroot before executing makes.
            char schroot_prefix[PATH_MAX];
            schroot_prefix[0] = 0;
            if (!schroot_user)
            {
                // Will be something like: schroot -c prefix_amd64 -d /build/dir/is/here/bin64/Debug -- 
                snprintf(schroot_prefix, sizeof(schroot_prefix), "schroot -c %s -d \"%s\" -- ", schroot_name[platind], build_dir);
                schroot_prefix[sizeof(schroot_prefix) - 1] = 0;
            }

            char set_compiler[PATH_MAX];
            const char *set_compiler_flag = " -q";

            if (args.flags & F_CLANG34)
                set_compiler_flag = " -c";
            else if (args.flags & F_CLANG33)
                set_compiler_flag = "";
            else if (args.flags & F_GCC48)
                set_compiler_flag = " -8";
            else if (args.flags & F_GCC)
                set_compiler_flag = " -g";

            snprintf(set_compiler, sizeof(set_compiler), "%s/bin/set_compiler.sh", vogl_proj_dir);
            systemf("exec %s%s%s", schroot_prefix, set_compiler, set_compiler_flag);

            // Always do the cmake command. Seems fast, and it rebuilds stuff if its changed.
            systemf("exec %scmake %s", schroot_prefix, defines.c_str());

            // Check if we are using Ninja or regular makefiles.
            char buildfile[PATH_MAX];

            snprintf(buildfile, sizeof(buildfile), "%s/%s", build_dir, "build.ninja");
            buildfile[sizeof(buildfile) - 1] = 0;

            bool use_ninja = (access(buildfile, F_OK) == 0);
            static const char *make_command = use_ninja ? "ninja" : "make -j 8";
            static const char *verbose_arg = use_ninja ? " -v" : " VERBOSE=1";

            systemf("exec %s%s%s", schroot_prefix, make_command, (args.flags & F_VERBOSE) ? verbose_arg : "");
        }
    }

    return 0;
}
