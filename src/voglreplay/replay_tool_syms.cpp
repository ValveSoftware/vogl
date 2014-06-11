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

// File: replay_tool_syms.cpp
#include "vogl_trace_file_reader.h"
#include "vogl_unique_ptr.h"

#if VOGL_PLATFORM_SUPPORTS_BTRACE
    #include "../libbacktrace/btrace.h"
#endif

#if !VOGL_PLATFORM_SUPPORTS_BTRACE

bool tool_symbols_mode(vogl::vector<command_line_param_desc> *desc)
{
    VOGL_FUNC_TRACER

    VOGL_VERIFY(!"impl tool_symbols_mode");
    return false;
}

#else

#define vogl_header_printf(...) vogl::console::printf(VOGL_FUNCTION_INFO_CSTR, cMsgPrint | cMsgFlagHeader, __VA_ARGS__)

//$ TODO: Need to run voglsyms32 to resolve 32-bit symbols and voglsyms64 for 64-bit symbols.
//        This is going to be a decent bit of work modifying elf.c in libbacktrace though...

//$ TODO: Need to update the tracefile with the resolved symbols.

//----------------------------------------------------------------------------------------------------------------------
// globals
//----------------------------------------------------------------------------------------------------------------------

struct addr_data_t
{
    uint32_t index;
    uint32_t count;
    vogl::vector<uintptr_t> addrs;
};

//----------------------------------------------------------------------------------------------------------------------
// command line params
//----------------------------------------------------------------------------------------------------------------------
static command_line_param_desc g_command_line_param_descs[] =
{
    { "resolve_symbols", 0, false, "Resolve symbols and write backtrace_map_syms.json to trace file" },
    { "loose_file_path", 1, false, "Prefer reading trace blob files from this directory vs. the archive referred to or present in the trace file" },
};

//----------------------------------------------------------------------------------------------------------------------
// dump_compiler_info
//----------------------------------------------------------------------------------------------------------------------
static bool dump_compiler_info(vogl_trace_file_reader *pTrace_reader, dynamic_string &tracefile_arch)
{
    if (pTrace_reader->get_archive_blob_manager().does_exist(VOGL_TRACE_ARCHIVE_COMPILER_INFO_FILENAME))
    {
        vogl_header_printf("%s\n", std::string(78, '*').c_str());
        vogl_header_printf("%s\n", VOGL_TRACE_ARCHIVE_COMPILER_INFO_FILENAME);
        vogl_header_printf("%s\n", std::string(78, '*').c_str());

        uint8_vec machine_info_data;
        if (pTrace_reader->get_archive_blob_manager().get(VOGL_TRACE_ARCHIVE_COMPILER_INFO_FILENAME, machine_info_data))
        {
            json_document doc;
            json_node *pRoot = doc.get_root();

            if (doc.deserialize((const char *)machine_info_data.get_ptr(), machine_info_data.size()) && pRoot->size())
            {
                for (uint i = 0; i < pRoot->size(); i++)
                {
                    const dynamic_string &sectionKeyStr = pRoot->get_key(i);

                    vogl_message_printf("%s\n", sectionKeyStr.c_str());
                    const vogl::json_value &value = pRoot->get_value(i);
                    const json_node *pNode = value.get_node_ptr();

                    // glinfo, uname, etc.
                    for (uint j = 0; j < pNode->size(); j++)
                    {
                        dynamic_string key = pNode->get_key(j);
                        const vogl::json_value &val = pNode->get_value(j);
                        dynamic_string str = val.as_string();

                        vogl_printf("  %s: %s\n", key.c_str(), str.c_str());

                        if (!key.compare("arch"))
                            tracefile_arch = str;
                    }
                }

                vogl_printf("\n");
                return true;
            }
        }
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// dump_machine_info
//----------------------------------------------------------------------------------------------------------------------
static bool dump_machine_info(vogl_trace_file_reader *pTrace_reader, vector<btrace_module_info> &module_infos)
{
    if (pTrace_reader->get_archive_blob_manager().does_exist(VOGL_TRACE_ARCHIVE_MACHINE_INFO_FILENAME))
    {
        vogl_header_printf("%s\n", std::string(78, '*').c_str());
        vogl_header_printf("%s\n", VOGL_TRACE_ARCHIVE_MACHINE_INFO_FILENAME);
        vogl_header_printf("%s\n", std::string(78, '*').c_str());

        uint8_vec machine_info_data;
        if (pTrace_reader->get_archive_blob_manager().get(VOGL_TRACE_ARCHIVE_MACHINE_INFO_FILENAME, machine_info_data))
        {
            json_document doc;
            json_node *pRoot = doc.get_root();

            if (doc.deserialize((const char *)machine_info_data.get_ptr(), machine_info_data.size()) && pRoot->size())
            {
                for (uint i = 0; i < pRoot->size(); i++)
                {
                    const dynamic_string &sectionKeyStr = pRoot->get_key(i);

                    const vogl::json_value &value = pRoot->get_value(i);
                    const json_node *pNode = value.get_node_ptr();

                    bool is_module_list = (sectionKeyStr == "module_list") && value.is_node();

                    if (!is_module_list)
                    {
                        vogl_message_printf("%s\n", sectionKeyStr.c_str());
                    }

                    if (is_module_list)
                    {
                        for (uint j = 0; j < pNode->size(); j++)
                        {
                            dynamic_string key = pNode->get_key(j);
                            const vogl::json_value &val = pNode->get_value(j);

                            // key is filename, addr_base, addr_size, uuid, is_exe
                            if (val.is_array())
                            {
                                uint index = 0;
                                const json_node *pNode2 = val.get_node_ptr();
                                uint size = pNode2->size();

                                btrace_module_info module_info;

                                memset(&module_info, 0, sizeof(module_info));

                                module_info.filename = vogl::vogl_strdup(key.c_str());
                                module_info.base_address = (index < size) ? (uintptr_t)pNode2->get_value(index++).as_uint64() : 0;
                                module_info.address_size = (index < size) ? pNode2->get_value(index++).as_uint32() : 0;
                                const char *uuid_str = (index < size) ? pNode2->get_value(index++).as_string_ptr() : "--";
                                module_info.is_exe = (index < size) ? pNode2->get_value(index++).as_string() == "(exe)" : false;

                                module_info.uuid_len = btrace_uuid_str_to_uuid(module_info.uuid, uuid_str);
                                module_infos.push_back(module_info);
                            }
                        }
                    }
                    else if (value.is_array())
                    {
                        // environ_list, cmdline, etc.
                        for (uint element = 0; element < pNode->size(); element++)
                        {
                            dynamic_string str = pNode->get_value(element).as_string();
                            vogl_printf("  %s\n", str.c_str());
                        }

                        vogl_printf("\n");
                    }
                    else if (value.is_node())
                    {
                        // glinfo, uname, etc.
                        for (uint j = 0; j < pNode->size(); j++)
                        {
                            dynamic_string key = pNode->get_key(j);
                            const vogl::json_value &val = pNode->get_value(j);

                            dynamic_string str = val.as_string();

                            if (val.is_array())
                            {
                                const json_node *pNode2 = val.get_node_ptr();

                                vogl_printf("%s\n", key.c_str());
                                for (uint element = 0; element < pNode2->size(); element++)
                                {
                                    dynamic_string str2 = pNode2->get_value(element).as_string();
                                    vogl_printf("  %s\n", str2.c_str());
                                }
                            }
                            else
                            {
                                vogl_printf("  %s: %s\n", key.c_str(), str.c_str());
                            }
                        }
                        vogl_printf("\n");
                    }
                    else
                    {
                        dynamic_string str = value.as_string();
                        vogl_printf("%s\n", str.c_str());
                    }
                }
            }

            return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// get_backtrace_map_addrs
//----------------------------------------------------------------------------------------------------------------------
static bool get_backtrace_map_addrs(vogl_trace_file_reader *pTrace_reader, vector<addr_data_t> &addr_data_arr)
{
    if (pTrace_reader->get_archive_blob_manager().does_exist(VOGL_TRACE_ARCHIVE_BACKTRACE_MAP_ADDRS_FILENAME))
    {
        uint8_vec backtrace_data_addrs;

        if (pTrace_reader->get_archive_blob_manager().get(VOGL_TRACE_ARCHIVE_BACKTRACE_MAP_ADDRS_FILENAME, backtrace_data_addrs))
        {
            json_document doc;
            json_node *pRoot = doc.get_root();

            if (doc.deserialize((const char *)backtrace_data_addrs.get_ptr(), backtrace_data_addrs.size()) && pRoot->size())
            {
                for (uint i = 0; i < pRoot->size(); i++)
                {
                    json_node *pChild = pRoot->get_child(i);
                    json_node *pAddrs = pChild->find_child_array("addrs");

                    addr_data_t addr_data;
                    addr_data.index = 0;
                    addr_data.count = 0;
                    pChild->get_value_as_uint32("index", addr_data.index);
                    pChild->get_value_as_uint32("count", addr_data.count);

                    for (uint j = 0; j < pAddrs->size(); j++)
                    {
                        uintptr_t addr = (uintptr_t)pAddrs->get_value(j).as_uint64();
                        addr_data.addrs.push_back(addr);
                    }

                    addr_data_arr.push_back(addr_data);
                }

                return true;
            }
        }
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// dump_backtrace_map_syms
//----------------------------------------------------------------------------------------------------------------------
static bool dump_backtrace_map_syms(vogl_trace_file_reader *pTrace_reader)
{
    if (pTrace_reader->get_archive_blob_manager().does_exist(VOGL_TRACE_ARCHIVE_BACKTRACE_MAP_SYMS_FILENAME))
    {
        uint8_vec backtrace_data;

        if (pTrace_reader->get_archive_blob_manager().get(VOGL_TRACE_ARCHIVE_BACKTRACE_MAP_SYMS_FILENAME, backtrace_data))
        {
            json_document doc;
            json_node *pRoot = doc.get_root();

            if (doc.deserialize((const char *)backtrace_data.get_ptr(), backtrace_data.size()) && pRoot->size())
            {
                vogl_header_printf("%s\n", std::string(78, '*').c_str());
                vogl_header_printf("%s\n", VOGL_TRACE_ARCHIVE_BACKTRACE_MAP_SYMS_FILENAME);
                vogl_header_printf("%s\n", std::string(78, '*').c_str());

                doc.print(true, 0, 0);
                return true;
            }
        }
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// voglsym_main_loop
//----------------------------------------------------------------------------------------------------------------------
bool tool_symbols_mode(vogl::vector<command_line_param_desc> *desc)
{
    VOGL_FUNC_TRACER

    if (desc)
    {
        desc->append(g_command_line_param_descs, VOGL_ARRAY_SIZE(g_command_line_param_descs));
        return true;
    }

    dynamic_string tracefile_arch;
    dynamic_string actual_trace_filename;
    vector<addr_data_t> addr_data_arr;
    vector<btrace_module_info> module_infos;

    dynamic_string trace_filename(g_command_line_params().get_value_as_string_or_empty("", 1));
    if (trace_filename.is_empty())
    {
        vogl_error_printf("No trace file specified!\n");
        return false;
    }

    vogl_unique_ptr<vogl_trace_file_reader> pTrace_reader(vogl_open_trace_file(
        trace_filename,
        actual_trace_filename,
        g_command_line_params().get_value_as_string_or_empty("loose_file_path").get_ptr()));
    if (!pTrace_reader.get())
    {
        vogl_error_printf("File not found, or unable to determine file type of trace file \"%s\"\n", trace_filename.get_ptr());
        return false;
    }

    bool resolve_symbols = g_command_line_params().get_value_as_bool("resolve_symbols");

    if (resolve_symbols)
        vogl_printf("Resolving symbols in trace file %s\n\n", actual_trace_filename.get_ptr());
    else
        vogl_printf("Reading trace file %s\n\n", actual_trace_filename.get_ptr());

    // compiler_info.json
    dump_compiler_info(pTrace_reader.get(), tracefile_arch);

    if (resolve_symbols)
    {
        if (tracefile_arch.size())
        {
            bool is_64bit = (sizeof(void *) == 8);
            bool trace_file_arch_is_64bits = !tracefile_arch.compare("64bit");

            if (trace_file_arch_is_64bits != is_64bit)
            {
                const char *arch_str = is_64bit ? "64-bit" : "32-bit";
                vogl_error_printf("ERROR: vogl tool is %s, tracefile is %s.\n", arch_str, tracefile_arch.c_str());
                vogl_error_printf("ERROR: Same architecture required to resolve symbols.\n");
                return -1;
            }
        }

        if (pTrace_reader->get_archive_blob_manager().does_exist(VOGL_TRACE_ARCHIVE_BACKTRACE_MAP_SYMS_FILENAME))
        {
            vogl_error_printf("ERROR: Symbols have already resolved for this tracefile.\n");
            return -1;
        }
    }

    // machine_info.json
    dump_machine_info(pTrace_reader.get(), module_infos);

    // backtrace_map_addrs.json
    get_backtrace_map_addrs(pTrace_reader.get(), addr_data_arr);

    // backtrace_map_syms.json
    dump_backtrace_map_syms(pTrace_reader.get());

    // Spew our module information
    if (module_infos.size())
    {
        vogl_header_printf("%s\n", std::string(78, '*').c_str());
        vogl_header_printf("%s\n", "Modules");
        vogl_header_printf("%s\n", std::string(78, '*').c_str());

        for (uint i = 0; i < module_infos.size(); i++)
        {
            char uuid_str[41];
            const btrace_module_info &module_info = module_infos[i];

            btrace_uuid_to_str(uuid_str, module_info.uuid, module_info.uuid_len);

            vogl_printf("0x%" PRIxPTR " (%u) %s %s %s",
                       module_info.base_address, module_info.address_size, uuid_str,
                       module_info.filename, module_info.is_exe ? "(exe)" : "");

            if (resolve_symbols)
            {
                const char *debug_filename = NULL;
                if (btrace_dlopen_add_module(module_info))
                {
                    debug_filename = btrace_get_debug_filename(module_info.filename);
                }

                if (debug_filename)
                {
                    vogl_printf(" [%s]", debug_filename);
                }
            }

            vogl_printf("\n");
        }

        vogl_printf("\n");
    }

    // Spew our backtrace addresses
    if (addr_data_arr.size())
    {
        vogl_header_printf("%s\n", std::string(78, '*').c_str());
        vogl_header_printf("%s\n", VOGL_TRACE_ARCHIVE_BACKTRACE_MAP_ADDRS_FILENAME);
        vogl_header_printf("%s\n", std::string(78, '*').c_str());

        vogl_printf("index (count): addr0, addr1, ...\n");
        for (uint i = 0; i < addr_data_arr.size(); i++)
        {
            const addr_data_t &addr_data = addr_data_arr[i];

            vogl_printf("0x%x (%u): ", addr_data.index, addr_data.count);
            for (uint j = 0; j < addr_data.addrs.size(); j++)
            {
                vogl_printf("%" PRIxPTR " ", addr_data.addrs[j]);
            }
            vogl_printf("\n");
        }

        vogl_printf("\n");
    }

    // Resolve symbols if we're supposed to.
    //$ TODO: The resolve symbols should be added to the trace file?
    if (resolve_symbols)
    {
        json_document doc;
        json_node *pRoot = doc.get_root();

        pRoot->init_array();

        vogl_header_printf("%s\n", std::string(78, '*').c_str());
        vogl_header_printf("%s\n", "Resolving symbols...");
        vogl_header_printf("%s\n", std::string(78, '*').c_str());

        for (uint i = 0; i < addr_data_arr.size(); i++)
        {
            const addr_data_t &addr_data = addr_data_arr[i];
            json_node &syms_arr = pRoot->add_array();

            for (uint j = 0; j < addr_data.addrs.size(); j++)
            {
                btrace_info trace_info;
                uintptr_t addr = addr_data.addrs[j];
                bool success = btrace_resolve_addr(&trace_info, addr,
                                                   BTRACE_RESOLVE_ADDR_GET_FILENAME | BTRACE_RESOLVE_ADDR_DEMANGLE_FUNC);

                dynamic_string sym;
                if (!success)
                {
                    sym = "?";
                }
                else if (trace_info.function[0] && trace_info.filename[0])
                {
                    // Got function and/or filename.
                    sym.format("%s (%s+0x%" PRIx64 ") at %s:%i",
                               trace_info.function,
                               trace_info.module[0] ? trace_info.module : "?",
                               cast_val_to_uint64(trace_info.offset),
                               trace_info.filename,
                               trace_info.linenumber);
                }
                else if (trace_info.function[0])
                {
                    // Got function, no filename.
                    sym.format("%s (%s+0x%" PRIx64 ")",
                               trace_info.function,
                               trace_info.module[0] ? trace_info.module : "?",
                               cast_val_to_uint64(trace_info.offset));
                }
                else
                {
                    // Only got modulename (no debugging information found).
                    sym.format("(%s+0x%" PRIx64 ")",
                               trace_info.module[0] ? trace_info.module : "?",
                               cast_val_to_uint64(trace_info.offset));
                }

                syms_arr.add_value(sym);
            }
        }

        doc.print(true, 0, 0);
    }

    return true;
}

#endif // defined(VOGL_PLATFORM_SUPPORTS_BTRACE)
