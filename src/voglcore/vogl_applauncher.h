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

#ifndef __VOGL_APPLAUNCHER_H__
#define __VOGL_APPLAUNCHER_H__

#include <unistd.h>
#include <stdio.h>
#include "vogl_dynamic_string.h"

// Example usage of this class. It launches glxinfo and grabs stdout.
#if 0
    vogl::app_launcher launcher;
    if (launcher.run("glxinfo", NULL, NULL, vogl::app_launcher::RUN_REMOVE_LD_PRELOAD_ENV))
    {
        vogl::dynamic_string output;
        if (launcher.get_stdout(output, 16384))
        {
            printf("got stdout: %s\n", output.c_str());
            launcher.stop();
        }
    }
#endif

namespace vogl
{

class app_launcher
{
public:
    app_launcher();
    ~app_launcher();

    enum
    {
        RUN_REMOVE_LD_PRELOAD_ENV = 0x00000001, // Parse envp and remove LD_PRELOAD.
    };
    bool run(const char *file, const char *const argv[], const char *const envp[], uint flags);
    bool stop();

    bool is_running();

    bool get_stdout(vogl::dynamic_string &output, size_t max_output = (size_t)-1); //SIZE_MAX);
    bool get_stderr(vogl::dynamic_string &output, size_t max_output = (size_t)-1); //SIZE_MAX);
    bool get_line(char **line_stdout, size_t *line_stdout_size, char **line_stderr, size_t *line_stderr_size);

    enum STATUS_CODE { STATUS_ERROR = -1, STATUS_NOTLAUNCHED, STATUS_RUNNING, STATUS_EXITED, STATUS_DUMPED, STATUS_KILLED };
    void get_status(STATUS_CODE *code, int *status, bool wait);

private:
    // Check for app exiting & update m_exit_status.
    bool update_status(bool wait);
    // Internal routine used for get_stdout and get_stderr.
    bool get_output(int pipe, vogl::dynamic_string &output, size_t max_output);

    STATUS_CODE m_status_code;
    int m_status;
    pid_t m_child_pid; // Pid of child (could be a script).

    int m_stdout_pipe[2];
    int m_stderr_pipe[2];
    FILE *m_stdout;
    FILE *m_stderr;
};

} // namespace vogl

#endif // __VOGL_APPLAUNCHER_H__
