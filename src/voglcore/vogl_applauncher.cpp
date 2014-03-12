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
#include <fcntl.h>
#include <assert.h>
#include <sys/wait.h>
#include "vogl_applauncher.h"
#include "base/posix/eintr_wrapper.h"

extern char **__environ;

namespace vogl
{

app_launcher::app_launcher()
{
    m_status_code = STATUS_NOTLAUNCHED;
    m_status = 0;
    m_child_pid = -1;

    m_stdout = NULL;
    m_stderr = NULL;

    memset(m_stdout_pipe, -1, sizeof(m_stdout_pipe));
    memset(m_stderr_pipe, -1, sizeof(m_stderr_pipe));
}

app_launcher::~app_launcher()
{
    stop();
}

sigset_t
set_signal_mask(const sigset_t& sigmask_new)
{
    sigset_t sigmask_old;
    pthread_sigmask(SIG_SETMASK, &sigmask_new, &sigmask_old);
    return sigmask_old;
}

bool
app_launcher::run(const char *file, const char *const argv[], const char *const envp[], uint flags)
{
    assert(!is_running());

    stop();
    m_status_code = STATUS_NOTLAUNCHED;
    m_status = 0;

    const char *argv_def[2];
    if (!argv)
    {
        argv_def[0] = file;
        argv_def[1] = NULL;
        argv = argv_def;
    }

    if (!envp)
    {
        envp = __environ;
    }

    vogl::vector<char *> new_envp;
    if (flags & RUN_REMOVE_LD_PRELOAD_ENV)
    {
        // Remove LD_PRELOAD. Need to do this to keep glxinfo from preloading us again (for example).
        static const char ld_preload_str[] = "LD_PRELOAD=";
        static const size_t ld_preload_len = sizeof(ld_preload_str) - 1;

        for(int i = 0; environ[i]; i++)
        {
            if (strncmp(ld_preload_str, environ[i], ld_preload_len))
                new_envp.push_back(environ[i]);
        }

        new_envp.push_back(NULL);
        envp = new_envp.get_ptr();
    }

    // [0]: read end, [1]: write end
    // Use pipe2 with O_CLOEXEC? O_NONBLK?
    int ret_stdout = pipe(m_stdout_pipe);
    int ret_stderr = pipe(m_stderr_pipe);
    if ((ret_stdout < 0) || (ret_stderr < 0))
        return false;

    sigset_t sigset_full;
    sigfillset(&sigset_full);
    const sigset_t sigmask_orig = set_signal_mask(sigset_full);

    // Careful: fork() can return pid of a script...
    m_child_pid = fork();

    if (m_child_pid != 0)
    {
        // Restore sigmask for parent.
        set_signal_mask(sigmask_orig);
    }

    if (m_child_pid < 0)
    {
        assert(0);
        m_status_code = STATUS_ERROR;
        m_status = m_child_pid;
        return false;
    }
    else if (m_child_pid == 0)
    {
        // According to chromium LaunchProcess(), readline processes can block forever
        //  unless we set stdin to /dev/null.
        int null_fd = HANDLE_EINTR(open("/dev/null", O_RDONLY));
        if (null_fd < 0)
            _exit(127);

        int new_stdin = HANDLE_EINTR(dup2(null_fd, STDIN_FILENO));
        ret_stdout = HANDLE_EINTR(dup2(m_stdout_pipe[1], STDOUT_FILENO));
        ret_stderr = HANDLE_EINTR(dup2(m_stderr_pipe[1], STDERR_FILENO));
        if ((new_stdin < 0) || (ret_stdout < 0) || (ret_stderr < 0))
            _exit(127);

        close(m_stdout_pipe[0]); // close read pipe
        close(m_stderr_pipe[0]); // close read pipe
        m_stdout_pipe[0] = -1;
        m_stderr_pipe[0] = -1;

        // exec file
        execvpe(file, (char * const *)argv, (char * const *)envp);
        _exit(127);
    }

    close(m_stdout_pipe[1]);
    close(m_stderr_pipe[1]);
    m_stdout_pipe[1] = -1;
    m_stderr_pipe[1] = -1;

    m_status_code = STATUS_RUNNING;
    return true;
}

bool
app_launcher::update_status(bool wait)
{
    if (m_status_code != STATUS_RUNNING)
        return false;

    assert(m_child_pid >= 0);

    int options = WEXITED; // WSTOPPED: wait for children stopped by delivery of signal.
    if(!wait)
        options |= WNOHANG;

    siginfo_t signalinfo;
    int ret = waitid(P_PID, m_child_pid, &signalinfo, options);

    if (ret == -1)
    {
        m_status_code = STATUS_ERROR;
        m_status = ret;
        assert(0);
        return false;
    }
    else
    {
        // If we ever care about signals and whatnot:
        //  signalinfo.si_signo;    // Signal number.
        //  signalinfo.si_code;     // Signal code: CLD_EXITED, CLD_DUMPED, CLD_KILLED.
        //  signalinfo.si_errno;    // Error number associated with signal.
        //  signalinfo.si_status;   // Exit value.
        switch(signalinfo.si_code)
        {
        default:
            assert(0);
        case CLD_EXITED:
            m_status_code = STATUS_EXITED;
            m_status = signalinfo.si_status;
            break;
        case CLD_KILLED:
            m_status_code = STATUS_KILLED;
            m_status = 0;
            break;
        case CLD_DUMPED:
            m_status_code = STATUS_DUMPED;
            m_status = 0;
            break;
        }
    }

    return true;
}

void
app_launcher::get_status(STATUS_CODE *code, int *status, bool wait)
{
    update_status(wait);

    *code = m_status_code;
    *status = m_status;
}

bool
app_launcher::is_running()
{
    update_status(false);

    return (m_status_code == STATUS_RUNNING);
}

bool
app_launcher::stop()
{
    update_status(true);
    if (m_status_code == STATUS_RUNNING)
    {
        kill(m_child_pid, SIGTERM);

        update_status(true);
        if (m_status_code == STATUS_RUNNING)
        {
            kill(m_child_pid, SIGKILL);
            update_status(true);
        }
    }
    assert(m_status_code != STATUS_RUNNING);

    if (m_stdout)
    {
        fclose(m_stdout);
        m_stdout = NULL;
    }
    if (m_stderr)
    {
        fclose(m_stderr);
        m_stderr = NULL;
    }

    for (int i = 0; i < 2; i++)
    {
        if (m_stdout_pipe[i] >= 0)
        {
            close(m_stdout_pipe[i]);
            m_stdout_pipe[i] = -1;
        }
        if (m_stderr_pipe[i] >= 0)
        {
            close(m_stderr_pipe[i]);
            m_stderr_pipe[i] = -1;
        }
    }

    return true;
}

bool
app_launcher::get_output(int pipe, vogl::dynamic_string &output, size_t max_output)
{
    if (pipe != -1)
    {
        if (max_output <= 0)
            return true;

        for(;;)
        {
            char buf[4096];
            size_t nbytes = max_output;
            if (nbytes >= sizeof(buf))
                nbytes = sizeof(buf);

            // Try to read in nbytes. read returns 0:end of file, -1:error.
            ssize_t length = HANDLE_EINTR(read(pipe, buf, nbytes));
            if (length < 0)
                return false;

            max_output -= length;
            output.append(buf, (uint)length);

            if ((length != (ssize_t)nbytes) || (max_output <= 0))
                return true;
        }
    }

    return false;
}

bool
app_launcher::get_stdout(vogl::dynamic_string &output, size_t max_output)
{
    return get_output(m_stdout_pipe[0], output, max_output);
}

bool
app_launcher::get_stderr(vogl::dynamic_string &output, size_t max_output)
{
    return get_output(m_stdout_pipe[1], output, max_output);
}

bool
app_launcher::get_line(char **line_stdout, size_t *line_stdout_size, char **line_stderr, size_t *line_stderr_size)
{
    if (!m_stdout)
    {
        m_stdout = fdopen(m_stdout_pipe[0], "r");
        setvbuf(m_stdout, NULL, _IONBF, 0);
    }
    if (!m_stderr)
    {
        m_stderr = fdopen(m_stderr_pipe[0], "r");
        setvbuf(m_stderr, NULL, _IONBF, 0);
    }
    if (!m_stdout || !m_stderr)
    {
        assert(0);
        return false;
    }

    *line_stdout_size = 0;
    *line_stderr_size = 0;

    fd_set set;
    FD_ZERO(&set);
    FD_SET(m_stdout_pipe[0], &set);
    FD_SET(m_stderr_pipe[0], &set);

    bool got_line = false;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int ret = select(FD_SETSIZE, &set, NULL, NULL, &timeout);
    if (ret < 0)
    {
        assert(0);
        return false;
    }
    if (!feof(m_stdout) && FD_ISSET(m_stdout_pipe[0], &set))
    {
        getline(line_stdout, line_stdout_size, m_stdout);
        got_line = true;
    }
    if (!feof(m_stderr) && FD_ISSET(m_stderr_pipe[0], &set))
    {
        getline(line_stderr, line_stderr_size, m_stderr);
        got_line = true;
    }

    return got_line;
}

} // namespace vogl
