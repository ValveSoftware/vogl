/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
 * Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC
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

// File: vogl_uuid.cpp
// FIXME: Obviously Unix/Linux specific stuff
#include "vogl_uuid.h"
#include "vogl_cfile_stream.h"
#include "vogl_threading.h"
#include "vogl_rand.h"

#if defined(PLATFORM_LINUX)
	#include <unistd.h>
	#include <sys/syscall.h>
	#include <sys/time.h>
	#include <pwd.h>
#elif defined(VOGL_USE_WIN32_API)
	#include <rpc.h>
    #pragma comment(lib, "Rpcrt4.lib")
#endif


#include <sys/types.h>
#include <time.h>

namespace vogl
{
    #if defined(PLATFORM_LINUX)
        // init_uuid() is slow (~40ms, maybe slower), and forces a disk flush on a file, so don't call it more than once.
        // I'm a paranoid nut so this hashes a bunch of shit. It's probably completely overkill for my needs - I should stop reading RFC's.
        static md5_hash init_uuid()
        {
            static uint64_t s_counter;

            // Get as much entropy as we can here

            const uint N = 2;
            void *p[N];
            memset(p, 0, sizeof(p));

            md5_hash_gen gen;
            timer_ticks tick_hist[N];
            for (uint i = 0; i < N; i++)
            {
                uint64_t start_rdtsc = utils::RDTSC();
                gen.update(start_rdtsc);

                gen.update(s_counter);
                gen.update((uint64_t) & s_counter);
                s_counter++;

                // Hash stack address of gen_uuid
                gen.update((uint64_t) & gen_uuid);

                // Hash the initial timer ticks, and time(NULL)
                gen.update(timer::get_init_ticks());
                gen.update((uint64_t)time(NULL));

                // Hash user ID, name, shell, home dir
                uid_t uid = geteuid();
                gen.update(uid);
                struct passwd *pw = getpwuid(uid);
                gen.update((uint64_t) & pw);
                if (pw)
                {
                    gen.update(pw, sizeof(struct passwd));
                    if (pw->pw_name)
                        gen.update(pw->pw_name, vogl_strlen(pw->pw_name));
                    if (pw->pw_passwd)
                        gen.update(pw->pw_passwd, vogl_strlen(pw->pw_passwd));
                    if (pw->pw_shell)
                        gen.update(pw->pw_shell, vogl_strlen(pw->pw_shell));
                    if (pw->pw_dir)
                        gen.update(pw->pw_dir, vogl_strlen(pw->pw_dir));
                    if (pw->pw_gecos)
                        gen.update(pw->pw_gecos, vogl_strlen(pw->pw_gecos));
                }

                uint8_vec buf;

                timer_ticks ticks = timer::get_ticks();
                gen.update(ticks);

                // This is obviously expensive (and questionable?), only do it once. But it helps us get some entropy from the disk subsystem.
                // This is also by far the slowest component of this function (~35ms out of ~40ms).
                if (!i)
                {
                    uint64_t st = utils::RDTSC();
                    timer tm;
                    tm.start();

                    const char *pFilename = "!_!_!_!_!_!_!_vogl_temp!_!_!_!_!_!_!_!_.txt";
                    FILE *pFile = vogl_fopen(pFilename, "wb");
                    gen.update_obj_bits(pFile);
                    if (pFile)
                    {
                        fwrite("X", 1, 1, pFile);
                        fflush(pFile);
                        fsync(fileno(pFile));
                        vogl_fclose(pFile);
                        remove(pFilename);
                    }

                    uint64_t t = utils::RDTSC() - st;
                    gen.update(t);

                    tm.stop();

                    gen.update(tm.get_elapsed_ticks());
                }

                // Grab some bits from /dev/urandom (not /dev/random - it may block for a long time)
                {
                    const uint N2 = 64;
                    char buf2[N2];
                    FILE *fp = vogl_fopen("/dev/urandom", "rb");
                    gen.update_obj_bits(fp);
                    if (fp)
                    {
                        size_t n = fread(buf2, 1, N2, fp);
                        VOGL_NOTE_UNUSED(n);
                        vogl_fclose(fp);

                        gen.update(buf2, sizeof(buf2));
                    }
                }

    // It's fine if some/most/all of these files don't exist, the true/false results get fed into the hash too.
    // TODO: Double check that all the files we should be able to read are actually getting read and hashed here.
    #define HASH_FILE(filename)                                               \
        do                                                                    \
        {                                                                     \
            bool success = cfile_stream::read_file_into_array(filename, buf); \
            gen.update_obj_bits(success);                                     \
            gen.update(buf);                                                  \
        } while (0)
                HASH_FILE("/proc/sys/kernel/random/entropy_avail");
                HASH_FILE("/proc/self/statm");
                HASH_FILE("/proc/self/mounts");
                HASH_FILE("/proc/self/io");
                HASH_FILE("/proc/self/smaps");
                HASH_FILE("/proc/self/stack");
                HASH_FILE("/proc/self/status");
                HASH_FILE("/proc/self/maps");
                HASH_FILE("/proc/self/stat");
                HASH_FILE("/proc/self/stat");
                HASH_FILE("/proc/cpuinfo");
                HASH_FILE("/proc/meminfo");
                HASH_FILE("/proc/stat");
                HASH_FILE("/proc/misc");
                HASH_FILE("/proc/swaps");
                HASH_FILE("/proc/version");
                HASH_FILE("/proc/loadavg");
                HASH_FILE("/proc/interrupts");
                HASH_FILE("/proc/ioports");
                HASH_FILE("/proc/partitions");
                HASH_FILE("/proc/driver/rtc");
                HASH_FILE("/proc/self/net/wireless");
                HASH_FILE("/proc/self/net/netstat");
                HASH_FILE("/proc/self/net/netlink");
                HASH_FILE("/sys/class/net/eth0/address");
                HASH_FILE("/sys/class/net/eth1/address");
                HASH_FILE("/sys/class/net/wlan0/address");
    #undef HASH_FILE

                gen.update(utils::RDTSC());

                // Hash thread, process ID's, etc.
                pid_t tid = (pid_t)syscall(SYS_gettid);
                gen.update_obj_bits(tid);

                pid_t pid = getpid();
                gen.update_obj_bits(pid);

                pid = getppid();
                gen.update_obj_bits(pid);
                gen.update((uint64_t) & pid);

                ticks -= timer::get_ticks();
                tick_hist[i] = ticks;
                gen.update(ticks);

                ticks = timer::get_ticks();

                // Get some entropy from the stack.
                char purposely_uninitialized_buf[256];
                gen.update(purposely_uninitialized_buf, sizeof(purposely_uninitialized_buf));

                // Get some entropy from the heap.
                p[i] = vogl_malloc(65536 * (i + 1));
                gen.update_obj_bits(p[i]);
                if (p[i])
                {
                    for (uint j = 0; j < 16; j++)
                        gen.update_obj_bits(reinterpret_cast<const uint64_t *>(p)[j]);
                }

                struct timeval tv;
                gettimeofday(&tv, NULL);
                gen.update_obj_bits(tv);

                // Hash the current environment
                uint e = 0;
                while (environ[e])
                {
                    gen.update(environ[e], vogl_strlen(environ[e]));
                    ++e;
                }

                uint64_t s = utils::RDTSC();

                // Try to get some entropy from the scheduler.
                vogl_sleep(2);

                gen.update(utils::RDTSC() - s);

                ticks -= timer::get_ticks();
                gen.update(ticks);

                gen.update(utils::RDTSC() - start_rdtsc);
            }

            for (uint i = 1; i < N; i++)
            {
                uint64_t t = tick_hist[i] - tick_hist[i - 1];
                gen.update(t);
            }

            for (uint i = 0; i < N; i++)
                vogl_free(p[i]);

            return gen.finalize();
        }
    #endif

    md5_hash gen_uuid()
    {
    #if defined(PLATFORM_LINUX)

        static bool s_uuid_initialized = false;
        static md5_hash s_uuid_key;
        static random s_uuid_rand;
        static mutex s_uuid_mutex;
        static md5_hash s_prev_uuid;

        uint64_t s = utils::RDTSC();

        scoped_mutex scoped_lock(s_uuid_mutex);

        if (!s_uuid_initialized)
        {
            s_uuid_key = init_uuid();

            s_uuid_rand.seed(s_uuid_key);

            s_uuid_initialized = true;
        }

        uint32 h[4] = { s_uuid_rand.urand32(), s_uuid_rand.urand32(), s_uuid_rand.urand32(), s_uuid_rand.urand32() };

        // Throw in some more quick sources of entropy, why not.
        md5_hash_gen gen(h, sizeof(h));
        gen.update_obj_bits(s_uuid_key);
        gen.update_obj_bits(s_prev_uuid);

        gen.update(s);
        gen.update((uint64_t) & s);

        uint64_t purposely_uninitialized_variable;
        gen.update(&purposely_uninitialized_variable, sizeof(purposely_uninitialized_variable));

        static uint64_t s_counter;
        s_counter++;
        gen.update(s_counter);

        struct timeval tv;
        gettimeofday(&tv, NULL);
        gen.update_obj_bits(tv);

        uint64_t e = utils::RDTSC();

        gen.update(e - s);

        return (s_prev_uuid = gen.finalize());

    #elif defined(VOGL_USE_WIN32_API)

        UUID windowsUuid;
        RPC_STATUS rpcStatus = UuidCreate(&windowsUuid);
        VOGL_VERIFY(SUCCEEDED(rpcStatus));

        return md5_hash(windowsUuid.Data1, windowsUuid.Data2, windowsUuid.Data3, windowsUuid.Data4);

    #else
        VOGL_ASSUME(!"Need implementation of gen_uuid for this platform.");
    #endif
    }

    uint64_t gen_uuid64()
    {
        md5_hash h(gen_uuid());
        return (static_cast<uint64_t>(h[0]) | (static_cast<uint64_t>(h[1]) << 32U)) ^
               (static_cast<uint64_t>(h[3]) | (static_cast<uint64_t>(h[2]) << 20U));
    }

} // namespace vogl
