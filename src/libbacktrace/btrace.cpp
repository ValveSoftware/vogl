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

//
// btrace.cpp
//
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <stdio.h>
#include <string.h>
#include <execinfo.h>
#include <link.h>
#include <sys/utsname.h>

// We need local unwinding only (should be more optimal than local + remote).
#define UNW_LOCAL_ONLY
#include <libunwind.h>

#include "btrace.h"
#include "backtrace.h"

#include "vogl_core.h"
#include "vogl_json.h"
#include "vogl_file_utils.h"

// our demangle routine from libelftc_dem_gnu3.c (from libcxxrt)
extern "C" char * __cxa_demangle_gnu3(const char *org);

// fast(er) backtrace routine from libunwind
extern "C" int unw_backtrace_skip (void **buffer, int size, int skip);

// Our dlopen mutex to protect g_module_infos operations
static vogl::mutex &get_dlopen_mutex()
{
    static vogl::mutex g_dlopen_mutex(0, true);
    return g_dlopen_mutex;
}

// An array of all the modules that have been loaded in this process.
static vogl::vector<btrace_module_info> &get_module_infos()
{
    static vogl::vector<btrace_module_info> s_module_infos;
    return s_module_infos;
}

btrace_glinfo& btrace_get_glinfo()
{
    static btrace_glinfo s_info;
    return s_info;
}

int
btrace_get(uintptr_t *addrs, size_t count_addrs, uint32_t addrs_to_skip)
{
#ifdef HAVE_UNW_BACKTRACE_SKIP
    return unw_backtrace_skip((void **)addrs, (int)count_addrs, addrs_to_skip);
#else
    size_t count = 0;
    unw_cursor_t cursor;
    unw_context_t context;

    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    while (count < count_addrs)
    {
        unw_word_t addr; 

        if (unw_step(&cursor) <= 0)
            break;

        unw_get_reg(&cursor, UNW_REG_IP, &addr);
        // Could retrieve registers via something like this:
        //   unw_get_reg(&cursor, UNW_X86_EAX, &eax), ...

        if (addrs_to_skip)
        {
            addrs_to_skip--;
            continue;
        }

        addrs[count++] = (uintptr_t)addr; 

#if 0
        // Get function name and offset from libunwind. Should match
        //  the libbacktrace code down below.
        unw_word_t offset;
        char function[512];
        function[0] = 0;
        unw_get_proc_name(&cursor, function, sizeof(function), &offset);
        printf ("0x%" PRIxPTR ": %s [+0x%" PRIxPTR "]\n", addr, function, offset);
#endif
    }

    return (int)count;
#endif
}

const char *
btrace_get_calling_module()
{
    unw_cursor_t cursor;
    unw_context_t context;
    const char *calling_fname = NULL;

    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    for(;;)
    {
        unw_word_t addr; 
        Dl_info dl_info;

        if (unw_step(&cursor) <= 0)
            break;

        unw_get_reg(&cursor, UNW_REG_IP, &addr);

        // Get module name.
        if (dladdr((void *)addr, &dl_info) == 0)
            return NULL;

        if (dl_info.dli_fname)
        {
            if (!calling_fname)
            {
                calling_fname = dl_info.dli_fname;
            }
            else if(strcmp(calling_fname, dl_info.dli_fname))
            {
                return dl_info.dli_fname;
            }
        }
    }

    return NULL;
}

static int
btrace_module_search (const void *vkey, const void *ventry)
{
    const uintptr_t *key = (const uintptr_t *)vkey;
    const struct btrace_module_info *entry = (const struct btrace_module_info *) ventry;
    uintptr_t addr;
 
    addr = *key;
    if (addr < entry->base_address)
        return -1;
    else if (addr >= entry->base_address + entry->address_size)
        return 1;
    return 0;
}

const char *
btrace_get_current_module()
{
    void *paddr = __builtin_return_address(0);
    uintptr_t addr = (uintptr_t)paddr;
    vogl::scoped_mutex lock(get_dlopen_mutex());

    // Try to search for the module name in our list. Should be faster than dladdr which
    //  goes through a bunch of symbol information.
    vogl::vector<btrace_module_info>& module_infos = get_module_infos();
    if (module_infos.size())
    {
        btrace_module_info *module_info = (btrace_module_info *)bsearch(&addr,
            &module_infos[0], module_infos.size(), sizeof(btrace_module_info), btrace_module_search);
        if (module_info && module_info->filename)
            return module_info->filename;
    }

    // Well, that failed for some reason. Try dladdr.
    Dl_info dl_info;
    if (dladdr(paddr, &dl_info) && dl_info.dli_fname)
        return dl_info.dli_fname;

    VOGL_ASSERT(0);
    return NULL;
}

static void
btrace_err_callback(void *data, const char *msg, int errnum)
{
    VOGL_NOTE_UNUSED(data);

    if (errnum == -1)
    {
        // Missing dwarf information. This happens when folks haven't compiled with -g or they
        //  stripped the symbols and we couldn't find em.
    }
    else
    {
        const char *errstr = errnum ? strerror(errnum) : "";
        printf("libbacktrace error: %s %s\n", msg, errstr);
        VOGL_ASSERT(0);
    }
}

static void
btrace_syminfo_callback(void *data, uintptr_t addr, const char *symname, uintptr_t symval, uintptr_t symsize)
{
    VOGL_NOTE_UNUSED(symsize);

    if (symname)
    {
        btrace_info *info = (btrace_info *)data;
        info->function = symname;
        info->offset = addr - symval;
    }
}

static int
btrace_pcinfo_callback(void *data, uintptr_t addr, const char *file, int line, const char *func)
{  
    VOGL_NOTE_UNUSED(addr);

    btrace_info *frame = (btrace_info *)data;

    frame->filename = file;
    frame->linenumber = line;

    // Don't overwrite function string if we got a blank one for some reason.
    if (func && func[0])
        frame->function = func; 
    return 0;
}

static void
backtrace_initialize_error_callback(void *data, const char *msg, int errnum)
{
    VOGL_NOTE_UNUSED(data);
    VOGL_NOTE_UNUSED(msg);
    VOGL_NOTE_UNUSED(errnum);
    // Backtrace_initialize only fails with alloc error and will be handled below.
}

static bool
module_info_init_state(btrace_module_info *module_info)
{
    if (!module_info->backtrace_state)
    {
        module_info->backtrace_state = backtrace_create_state(
                    module_info->filename, 0, backtrace_initialize_error_callback, NULL);
        if (module_info->backtrace_state)
        {
            elf_get_uuid(module_info->backtrace_state, module_info->filename,
                         module_info->uuid, &module_info->uuid_len);
        }
    }

    return !!module_info->backtrace_state;
}

bool
btrace_resolve_addr(btrace_info *info, uintptr_t addr, uint32_t flags)
{
    vogl::scoped_mutex lock(get_dlopen_mutex());
    vogl::vector<btrace_module_info>& module_infos = get_module_infos();
 
    if (!module_infos.size())
        btrace_dlopen_notify(NULL);

    info->addr = addr;
    info->offset = 0;
    info->module = NULL;
    info->function = NULL;
    info->filename = NULL;
    info->linenumber = 0;
    info->demangled_func_buf[0] = 0;

    btrace_module_info *module_info = (btrace_module_info *)bsearch(&addr,
        &module_infos[0], module_infos.size(), sizeof(btrace_module_info), btrace_module_search);
    if (module_info)
    {
        info->module = module_info->filename;

        if (module_info_init_state(module_info))
        {
            backtrace_fileline_initialize(module_info->backtrace_state, module_info->base_address,
                                          module_info->is_exe, backtrace_initialize_error_callback, NULL);

            // Get function name and offset.
            backtrace_syminfo(module_info->backtrace_state, addr, btrace_syminfo_callback,
                              btrace_err_callback, info);

            if (flags & BTRACE_RESOLVE_ADDR_GET_FILENAME)
            {
                // Get filename and line number (and maybe function).
                backtrace_pcinfo(module_info->backtrace_state, addr, btrace_pcinfo_callback,
                                 btrace_err_callback, info); 
            }

            if ((flags & BTRACE_RESOLVE_ADDR_DEMANGLE_FUNC) && info->function && info->function[0])
            {
                info->function = btrace_demangle_function(info->function, info->demangled_func_buf, sizeof(info->demangled_func_buf));
            }
        }

        if (!info->offset)
            info->offset = addr - module_info->base_address;
    }

    // Get module name.
    if (!info->module || !info->module[0])
    {
        Dl_info dl_info;
        if (dladdr((void *)addr, &dl_info))
            info->module = dl_info.dli_fname;
        if (!info->offset)
            info->offset = addr - (uintptr_t)dl_info.dli_fbase;
    }

    if (info->module)
    {
        const char *module_name = strrchr(info->module, '/');
        if (module_name)
            info->module = module_name + 1;
    }

    if (!info->module)
        info->module = "";
    if (!info->function)
        info->function = "";
    if (!info->filename)
        info->filename = "";
    return 1; 
}

static int
get_hex_value(char ch)
{
    if (ch >= 'A' && ch <= 'F')
        return 10 + ch - 'A';
    else if (ch >= 'a' && ch <= 'f')
        return 10 + ch - 'a';
    else if (ch >= '0' && ch <= '9')
        return ch - '0';

    return -1;
}

int
btrace_uuid_str_to_uuid(uint8_t uuid[20], const char *uuid_str)
{
    int len;

    for (len = 0; (len < 20) && *uuid_str; len++)
    {
        int val0 = get_hex_value(*uuid_str++);
        int val1 = get_hex_value(*uuid_str++);
        if (val0 < 0 || val1 < 0)
            break;
        uuid[len] = (val0 << 4) | val1;
    }

    return len;
}

void
btrace_uuid_to_str(char uuid_str[41], const uint8_t *uuid, int len) 
{
    int i;
    static const char hex[] = "0123456789abcdef";

    if (len > 40)
        len = 40;
    for (i = 0; i < len; i++) 
    {
        uint8_t c = *uuid++;

        *uuid_str++ = hex[c >> 4];
        *uuid_str++ = hex[c & 0xf];
    }    
    *uuid_str = 0;
}

bool
btrace_get_machine_info(vogl::json_node *machine_info)
{
    vogl::scoped_mutex lock(get_dlopen_mutex());
    vogl::vector<btrace_module_info>& module_infos = get_module_infos();
 
    if (!module_infos.size())
        btrace_dlopen_notify(NULL);

    // Add the module list.
    vogl::json_node &module_list_node = machine_info->add_array("module_list");
    for (uint i = 0; i < module_infos.size(); i++)
    {
        btrace_module_info &module_info = module_infos[i];
        char uuid_str[sizeof(module_info.uuid) * 2 + 1];

        // Make sure we've gotten the uuid.
        module_info_init_state(&module_info);

        btrace_uuid_to_str(uuid_str, module_info.uuid, sizeof(module_info.uuid));

        vogl::json_node &arr = module_list_node.add_array(module_info.filename);
        arr.add_value(module_info.base_address);
        arr.add_value(module_info.address_size);
        arr.add_value(uuid_str);
        
        if (module_info.is_exe)
            arr.add_value("(exe)");
    }

    // Add the environment variables.
    vogl::growable_array<char, 2048> proc_file;
    if (vogl::file_utils::read_proc_file("/proc/self/environ", proc_file))
    {
        vogl::json_node &env_list_node = machine_info->add_array("environ_list");

        char *ptr = proc_file.get_ptr();
        uint size = proc_file.size_in_bytes();
        char *var = ptr;
        while ((var < ptr + size) && *var)
        {
            env_list_node.add_value(var);
            var += strlen(var) + 1;
        }
    }

    if (vogl::file_utils::read_proc_file("/proc/self/cmdline", proc_file))
    {
        vogl::json_node &env_list_node = machine_info->add_array("cmdline");

        char *ptr = proc_file.get_ptr();
        uint size = proc_file.size_in_bytes();
        char *var = ptr;
        while ((var < ptr + size) && *var)
        {
            env_list_node.add_value(var);
            var += strlen(var) + 1;
        }
    }

    utsname uname_data;
    if (!uname(&uname_data))
    {
        vogl::json_node &env_list_node = machine_info->add_array("uname");

#define ADD_KEY_VALUE(_val) if (uname_data._val) env_list_node.add_key_value(#_val, uname_data._val);
        ADD_KEY_VALUE(sysname);
        ADD_KEY_VALUE(nodename);
        ADD_KEY_VALUE(release);
        ADD_KEY_VALUE(version);
        ADD_KEY_VALUE(machine);
#undef ADD_KEY_VALUE
    }
    
    // Should be tsc or hpet (if TSC failed to calibrate).
    if (vogl::file_utils::read_proc_file("/sys/devices/system/clocksource/clocksource0/current_clocksource", proc_file))
    {
        vogl::json_node &env_list_node = machine_info->add_array("current_clocksource");
        env_list_node.add_value(proc_file.get_ptr());
    }

    if (vogl::file_utils::read_proc_file("/proc/driver/nvidia/version", proc_file))
    {
        vogl::json_node &env_list_node = machine_info->add_array("nvidia_version");
        env_list_node.add_value(proc_file.get_ptr());
    }
    if (vogl::file_utils::read_proc_file("/proc/driver/nvidia/gpus/0/information", proc_file))
    {
        vogl::json_node &env_list_node = machine_info->add_array("nvidia_gpus_0_information");
        env_list_node.add_value(proc_file.get_ptr());
    }

    btrace_glinfo &glinfo = btrace_get_glinfo();
    vogl::json_node &glinfo_list_node = machine_info->add_array("glinfo");
    glinfo_list_node.add_key_value("version_str", glinfo.version_str);
    glinfo_list_node.add_key_value("glsl_version_str", glinfo.glsl_version_str);
    glinfo_list_node.add_key_value("vendor_str", glinfo.vendor_str);
    glinfo_list_node.add_key_value("renderer_str", glinfo.renderer_str);

    return true;
}

int
btrace_dump()
{
    int i;
    uintptr_t addrs[128];
    int count = btrace_get(addrs, VOGL_ARRAY_SIZE(addrs), 0);

    for (i = 0; i < count; i++)
    {
        int ret;
        btrace_info info;

        ret = btrace_resolve_addr(&info, addrs[i],
                                BTRACE_RESOLVE_ADDR_GET_FILENAME | BTRACE_RESOLVE_ADDR_DEMANGLE_FUNC);

        printf(" 0x%" PRIxPTR " ", addrs[i]);
        if (ret)
        {
            printf("%s", info.module);

            if (info.function[0])
            {
                printf(": %s", info.function);
            }

            printf("+0x%" PRIxPTR, info.offset);

            if (info.filename && info.filename[0])
            {
                // Print last directory plus filename if possible.
                const char *slash_cur = info.filename;
                const char *slash_last = NULL;
                for (const char *ch = info.filename; *ch; ch++)
                {
                    if (*ch == '/')
                    {
                        slash_last = slash_cur;
                        slash_cur = ch + 1;
                    }
                }
                const char *filename = slash_last ? slash_last : slash_cur;
                printf(": %s:%d", filename, info.linenumber); 
            }
        }

        printf("\n");
    }

    return count;
}

static int
module_info_less_than_func(const btrace_module_info &key0, const btrace_module_info &key1)
{
    //$ TODO: We can get rid of this function and it'll just call the operator< func?
    return key0 < key1;
}

static int
dlopen_notify_callback(struct dl_phdr_info *info, size_t size, void *data)
{
    VOGL_NOTE_UNUSED(size);

    char buf[PATH_MAX];
    bool is_exe = false;
    const char *filename = info->dlpi_name;
    vogl::vector<btrace_module_info> *new_module_infos = (vogl::vector<btrace_module_info> *)data;
    vogl::vector<btrace_module_info>& module_infos = get_module_infos();

    // If we don't have a filename and we haven't added our main exe yet, do it.
    if (!filename || !filename[0])
    {
        if (!module_infos.size() && !new_module_infos->size())
        {
            is_exe = true;
            ssize_t ret =  readlink("/proc/self/exe", buf, sizeof(buf));
            if ((ret > 0) && (ret < (ssize_t)sizeof(buf)))
            {
                buf[ret] = 0;
                filename = buf;
            }
        }
        if (!filename || !filename[0])
            return 0;
    }

    uintptr_t addr_start = 0;
    uintptr_t addr_end = 0;

    for (int i = 0; i < info->dlpi_phnum; i++)
    {
        if (info->dlpi_phdr[i].p_type == PT_LOAD)
        {
            if (addr_end == 0)
            {
                addr_start = info->dlpi_addr + info->dlpi_phdr[i].p_vaddr; 
                addr_end = addr_start + info->dlpi_phdr[i].p_memsz;
            }
            else
            {
                uintptr_t addr = info->dlpi_addr + info->dlpi_phdr[i].p_vaddr + info->dlpi_phdr[i].p_memsz; 
                if (addr > addr_end)
                    addr_end = addr;
            }
        }
    }

    btrace_module_info module_info;
    module_info.base_address = addr_start;
    module_info.address_size = (uint32_t)(addr_end - addr_start);
    module_info.filename = filename;
    module_info.is_exe = is_exe;
    module_info.backtrace_state = NULL;
    module_info.uuid_len = 0;
    memset(module_info.uuid, 0, sizeof(module_info.uuid));

    int ret = module_infos.find_sorted(module_info, module_info_less_than_func);
    if (ret == vogl::cInvalidIndex)
    {
        module_info.filename = vogl::vogl_strdup(filename);
        if (module_info.filename)
        {
            new_module_infos->push_back(module_info);
        }
    }
    return 0;
}

bool
btrace_dlopen_add_module(const btrace_module_info &module_info_in)
{
    vogl::scoped_mutex lock(get_dlopen_mutex());
    vogl::vector<btrace_module_info>& module_infos = get_module_infos();

    int ret = module_infos.find_sorted(module_info_in, module_info_less_than_func);
    if (ret == vogl::cInvalidIndex)
    {
        btrace_module_info module_info = module_info_in;

        if (module_info_init_state(&module_info))
        {
            // Make sure the UUID of the file on disk matches with what we were asked for.
            if ((module_info_in.uuid_len == module_info.uuid_len) &&
                    !memcmp(module_info_in.uuid, module_info.uuid, module_info.uuid_len))
            {
                module_infos.push_back(module_info);
                module_infos.sort();
                return true;
            }
        }
    }

    return false;
}

const char *
btrace_get_debug_filename(const char *filename)
{
    vogl::scoped_mutex lock(get_dlopen_mutex());
    vogl::vector<btrace_module_info>& module_infos = get_module_infos();
   
    vogl::dynamic_string fname = filename;
    for (uint i = 0; i < module_infos.size(); i++)
    {
        btrace_module_info &module_info = module_infos[i];

        if (fname.compare(module_info.filename, true) == 0)
        {
            if (module_info_init_state(&module_info))
            {
                backtrace_fileline_initialize(module_info.backtrace_state, module_info.base_address,
                                              module_info.is_exe, backtrace_initialize_error_callback, NULL);
                
                return backtrace_get_debug_filename(module_info.backtrace_state);
            }
        }
    }
    return NULL;
}

// This function is called from a dlopen hook, which means it could be
//  called from the driver or other code which hasn't aligned the stack.
#ifdef __i386
void __attribute__((force_align_arg_pointer)) 
#else
void
#endif
btrace_dlopen_notify(const char *filename)
{
    VOGL_NOTE_UNUSED(filename);

    // Make sure the vogl heap is initialized.
    vogl::vogl_init_heap();

    vogl::scoped_mutex lock(get_dlopen_mutex());
    vogl::vector<btrace_module_info> new_module_infos;

    // Iterator through all the currently loaded modules.
    dl_iterate_phdr(dlopen_notify_callback, &new_module_infos);

    if (new_module_infos.size())
    {
        vogl::vector<btrace_module_info>& module_infos = get_module_infos();
        module_infos.append(new_module_infos);
        module_infos.sort();
    }
}

//
// Other possibilities:
//   abi::__cxa_demangle() (cxxabi.h)
//   bfd_demangle (bfd.h)
//   cplus_demangle (demangle.h) libiberty code from gcc
//
// #include <cxxabi.h>
// char * abi::__cxa_demangle(const char* __mangled_name, char* __output_buffer,
//    size_t* __length, int* __status);
// int status = 0;
// char *function = abi::__cxa_demangle(name, NULL, NULL, &status);
//
const char *
btrace_demangle_function(const char *name, char *buffer, size_t buflen)
{
    char *function = NULL;

    // Mangled-name is function or variable name...
    if (name[0] == '_' && name[1] == 'Z')
        function = __cxa_demangle_gnu3(name);

    if (function && function[0])
        snprintf(buffer, buflen, "%s", function);
    else
        snprintf(buffer, buflen, "%s", name);

    buffer[buflen - 1] = 0;
    free(function); 
    return buffer;
}

#if 0

// Based on code from this newsgroup:
//   https://groups.google.com/forum/#!topic/comp.unix.programmer/paDVOCaLP7g
//
// And this spec:
//   http://mentorembedded.github.io/cxx-abi/abi.html#mangling
static const char *get_op_name(char op1, char op2)
{
    static const struct op_overloads
    {
        char       op_suffix[2];
        const char *op_name;
    } s_op_overloads[] =
    {
        //$ TODO mikesart: missing these:
        { { 'n', 'w' }, " new" },
        { { 'n', 'a' }, " new[]" },
        { { 'd', 'l' }, " delete" },
        { { 'd', 'a' }, " delete[]" },
        { { 'p', 's' }, "+(unary)" },
        { { 'n', 'g' }, "-(unary)" },
        { { 'd', 'e' }, "*(unary)" },
        { { 'a', 'd' }, "&(unary)" },
        { { 'c', 'o' }, "~" },
        { { 'p', 'l' }, "+" },
        { { 'm', 'i' }, "-" },
        { { 'm', 'l' }, "*" },
        { { 'd', 'v' }, "/" },
        { { 'r', 'm' }, "%" },
        { { 'a', 'n' }, "&" },
        { { 'o', 'r' }, "|" },
        { { 'e', 'o' }, "^" },
        { { 'a', 'S' }, "=" },
        { { 'p', 'L' }, "+=" },
        { { 'm', 'I' }, "-=" },
        { { 'm', 'L' }, "*=" },
        { { 'd', 'V' }, "/=" },
        { { 'r', 'M' }, "%=" },
        { { 'a', 'N' }, "&=" },
        { { 'o', 'R' }, "|=" },
        { { 'e', 'O' }, "^=" },
        { { 'l', 's' }, "<<" },
        { { 'r', 's' }, ">>" },
        { { 'l', 'S' }, "<<=" },
        { { 'r', 'S' }, ">>=" },
        { { 'e', 'q' }, "==" },
        { { 'n', 'e' }, "!=" },
        { { 'l', 't' }, "<" },
        { { 'g', 't' }, ">" },
        { { 'l', 'e' }, "<=" },
        { { 'g', 'e' }, ">=" },
        { { 'n', 't' }, "!" },
        { { 'a', 'a' }, "&&" },
        { { 'o', 'o' }, "||" },
        { { 'p', 'p' }, "++" },
        { { 'm', 'm' }, "--" },
        { { 'c', 'm' }, "," },
        { { 'p', 'm' }, "->*" },
        { { 'p', 't' }, "->" },
        { { 'c', 'l' }, "()" },
        { { 'i', 'x' }, "[]" },
        { { 'q', 'u' }, "?" },
        { { 'c', 'v' }, "(cast)" }, // cv <type>        # cast
        { { 'l', 'i' }, "\"\"" },   // li <source-name> # operator ""
        { { 0, 0 }, NULL }
    };
    const struct op_overloads *op;

    for (op = s_op_overloads; op->op_name; op++)
    {
        if (op1 == op->op_suffix[0] && op2 == op->op_suffix[1])
            return op->op_name;
    }
    return NULL;
}

static const char *read_len(const char *src, int *len)
{
    int val;

    for (val = 0; (*src >= '0') && (*src <= '9'); src++)
    {
        val *= 10;
        val += (*src - '0');
    }

    *len = val;
    return src;
}

void add_char(char **dst, size_t *buflen, char ch)
{
    if (*buflen > 0)
    {
        *(*dst)++ = ch;
        (*buflen)--;
    }
}

const char *add_string(char **dst, size_t *buflen, const char *str, size_t len)
{
    if (len && (*buflen > len))
    {
        const char *ret = str + len;

        *buflen -= len;
        while (len-- > 0)
            *(*dst)++ = *str++; 
        return ret;
    }

    return str;
}

const char *
btrace_demangle_function(const char *name, char *buffer, size_t buflen)
{
    int i;
    int srclen;
    const char *src;
    char *dst = buffer;
    char *destbak;
    int is_vtbl = 0;
    int is_guard = 0;
    int do_parens = 1;
    int is_thunk = 0;
    int is_thunk_neg = 0;
    int thunk_offset = 0;
    int is_nested_name = 0;
    size_t buflen_orig = buflen;
/*
    name = "_ZN6google8protobuf16strtou32_adaptorEPKcPPci";
    name = "_ZN6google8protobuf24SimpleDescriptorDatabase15DescriptorIndexIPKNS0_19FileDescriptorProtoEE12AddExtensionERKNS0_20FieldDescriptorProtoES5_";
    name = "_ZN6google8protobuf7strings10SubstituteEPKcRKNS1_8internal13SubstituteArgES7_S7_S7_S7_S7_S7_S7_S7_S7_";
    name = "_ZN6google8protobuf8compiler28SourceTreeDescriptorDatabase24ValidationErrorCollector8AddErrorERKSsS5_PKNS0_7MessageENS0_14DescriptorPool14ErrorCollector13ErrorLocationES5_";
    name = "_ZNSt3mapISsSsSt4lessISsESaISt4pairIKSsSsEEED1Ev";
    name = "_ZNSt3tr110_HashtableISt4pairIPKN6google8protobuf11MessageLiteEiES1_IKS7_NS3_8internal13ExtensionInfoEESaISB_ESt10_Select1stISB_ESt8equal_toIS7_ENS3_4hashIS7_EENS_8__detail18_Mod_range_hashingENSJ_20_Default_ranged_hashENSJ_20_Prime_rehash_policyELb0ELb0ELb1EE9_M_insertERKSB_NS_17integral_constantIbLb1EEE";
    */

    // Mangled-name is function or variable name...
    if (name[0] != '_' || name[1] != 'Z')
    {
        snprintf(buffer, buflen, "%s", name);
        buffer[buflen - 1] = 0;
        return buffer;
    }

    // Skip over '_Z'
    src = name + 2;

    if (src[0] == 'N')
    {
        // nested-name
        src++;
        is_nested_name = 1;
    }
    else if (src[0] == 'T' && src[1] == 'V')
    {
        // virtual table
        src += 2;
        is_vtbl = 1;

        if (src[0] == 'N')
        {
            src++;
            is_nested_name = 1;
        }
    }
    else if (src[0] == 'T' && src[1] == 'h')
    {
        // call-offset
        src += 2;

        if (*src == 'n')
        {
            is_thunk_neg = 1;
            src++;
        }

        src = read_len(src, &thunk_offset);

        // Should be skipping over '_' and something else?
        src += 2;
        is_thunk = 1;
    }
    else if (src[0] == 'G' && src[1] == 'V')
    {
        // Guard variable for one-time initialization
        src += 3; //$ TODO: Should this be += 3???
        if (*src == 'N')
            src++;
        is_guard = 1;
    }

    destbak = dst;

    src = read_len(src, &srclen);
    src = add_string(&dst, &buflen, src, srclen);

    // _ZN3nsA3nsB3nsC3xxxD2Ev == nsA::nsB::nsC::xxx::~xxx()
    while (is_nested_name)
    {
        if (src[0] == 'S' && src[1] == 't')
        {
            add_string(&dst, &buflen, "std", 3);
            src += 2;
        }

        src = read_len(src, &srclen); 
        if (!srclen)
            break;

        add_string(&dst, &buflen, "::", 2);
        destbak = dst;

        src = add_string(&dst, &buflen, src, srclen);

        is_nested_name = (src[0] >= '1') && (src[0] <= '9');
    }

    if (is_vtbl)
    {
        add_string(&dst, &buflen, "::[vtbl]", 8);
        do_parens = 0;
    }
    else if (is_guard)
    {
        add_string(&dst, &buflen, "::[GuardVar]", 12);
        do_parens = 0;
    }
    else if (src[0] == 'C' || src[0] == 'D')
    {
        // <ctor-dtor-name> ::= C1	# complete object constructor
        //                  ::= C2	# base object constructor
        //                  ::= C3	# complete object allocating constructor
        //                  ::= D0	# deleting destructor
        //                  ::= D1	# complete object destructor
        //                  ::= D2	# base object destructor
        add_string(&dst, &buflen, "::", 2);

        if (*src == 'D')
            add_char(&dst, &buflen, '~');

        add_string(&dst, &buflen, destbak, srclen);
    }
    else if (src[0] >= '0' && *src <= '9')
    {
        add_string(&dst, &buflen, "::", 2);

        if (is_thunk)
        {
            char thunk[64];
            snprintf(thunk, sizeof(thunk), "[thunk%c%d]", is_thunk_neg ? '-' : '+', thunk_offset);
            thunk[sizeof(thunk) - 1] = 0;

            add_string(&dst, &buflen, thunk, strlen(thunk));
        }

        src = read_len(src, &srclen);
        src = add_string(&dst, &buflen, src, srclen);

        if (*src != 'E')
            do_parens = 0;
    }
    else if (src[0] == 'c' && src[1] == 'v')
    {
        src += 2;

        add_string(&dst, &buflen, "::operator (", 12);

        src = read_len(src, &srclen);
        src = add_string(&dst, &buflen, src, srclen);

        add_char(&dst, &buflen, ')');
    }
    else
    {
        const char *op_name = get_op_name(src[0], src[1]);
        if (op_name)
        {
            add_string(&dst, &buflen, "::operator", 10); 
            add_string(&dst, &buflen, op_name, strlen(op_name));
        }
    }

    if (do_parens)
    {
        add_string(&dst, &buflen, "()", 2);
    }

    add_char(&dst, &buflen, '\0');
    buffer[buflen_orig - 1] = 0;
    return buffer;
}

#endif
