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

/*
 * libbacktrace_test.cpp
 */
#include <stdio.h>
#include "btrace.h"
#include "vogl_core.h"
#include "vogl_json.h"

namespace blahnamespace
{
    class blah
    {
    public:
        blah() { }
        ~blah() { }
        void foobar(int x, const char *y, double z);
        inline void foobar2()
        {
            foobar(0, "", 0.0);
        }
    };

    void blah::foobar(int x, const char *y, double z)
    {
        VOGL_NOTE_UNUSED(x);
        VOGL_NOTE_UNUSED(y);
        VOGL_NOTE_UNUSED(z);

        btrace_dump();

        const char *calling_module = btrace_get_calling_module();
        const char *current_module = btrace_get_current_module();

        printf("\nbtrace_get_calling_module returns: %s\n", calling_module ? calling_module : "??");
        printf("\nbtrace_get_current_module returns: %s\n", current_module ? current_module : "??");
    }
}

void foo()
{
    blahnamespace::blah b;

    b.foobar2();
}

typedef void (PFN)(void);
extern int yyy(PFN *pfn);

inline void foo_inlined()
{
    yyy(foo);
}

int foo_static()
{
    foo_inlined();
    return 0;
}

int main(int argc, char *argv[])
{
    VOGL_NOTE_UNUSED(argc);
    VOGL_NOTE_UNUSED(argv);
  
    printf("libbacktrace_test: hello world!\n\n");

    foo_static();

    printf("\n");

    // Update btrace with our gl info.
    btrace_get_glinfo().version_str = "version string here";
    btrace_get_glinfo().glsl_version_str = "glsl version 4 billion";
    btrace_get_glinfo().vendor_str = "vendor";
    btrace_get_glinfo().renderer_str = "renderer_str";

    vogl::json_document cur_doc;
    btrace_get_machine_info(cur_doc.get_root());
    cur_doc.print(true, 0, 0);
}
