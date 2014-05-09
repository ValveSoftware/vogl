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

// File: vogl_blob_manager.h
#ifndef VOGL_BLOB_MANAGER_H
#define VOGL_BLOB_MANAGER_H

#include "vogl_map.h"
#include "vogl_data_stream.h"
#include "vogl_miniz_zip.h"

enum vogl_blob_manager_type_t
{
    cBMTNull,
    cBMTFile,
    cBMTMemory,
    cBMTArchive,
    cBMTMulti
};

enum vogl_blob_manager_flags_t
{
    cBMFReadable = 1,
    cBMFWritable = 2,
    cBMFReadWrite = cBMFReadable | cBMFWritable,
    cBMFOpenExisting = 4,
    cBMFOpenExistingOrCreateNew = 8,
};

//----------------------------------------------------------------------------------------------------------------------
// class vogl_blob_manager
//----------------------------------------------------------------------------------------------------------------------
class vogl_blob_manager
{
public:
    vogl_blob_manager();
    virtual ~vogl_blob_manager();

    virtual bool deinit()
    {
        m_flags = 0;
        m_initialized = false;
        return true;
    }

    virtual bool is_initialized() const
    {
        return m_initialized;
    }

    virtual bool populate(const vogl_blob_manager &other);

    virtual vogl_blob_manager_type_t get_type() const = 0;

    vogl::dynamic_string compute_unique_id(const void *pData, uint32_t size, const vogl::dynamic_string &prefix = "", const dynamic_string &ext = "", const uint64_t *pCRC64 = NULL) const;
    vogl::dynamic_string get_prefix(const vogl::dynamic_string &id) const;
    vogl::dynamic_string get_extension(const vogl::dynamic_string &id) const;

    virtual bool get(const dynamic_string &id, vogl::uint8_vec &data) const;

    virtual vogl::dynamic_string add_buf_compute_unique_id(const void *pData, uint32_t size, const vogl::dynamic_string &prefix, const dynamic_string &ext, const uint64_t *pCRC64 = NULL);
    virtual vogl::dynamic_string add_stream_compute_unique_id(vogl::data_stream &stream, const vogl::dynamic_string &prefix, const dynamic_string &ext, const uint64_t *pCRC64 = NULL);

    virtual vogl::dynamic_string add_stream_using_id(vogl::data_stream &stream, const vogl::dynamic_string &id);

    virtual vogl::dynamic_string add_buf_using_id(const void *pData, uint32_t size, const vogl::dynamic_string &id) = 0;

    virtual vogl::data_stream *open(const vogl::dynamic_string &id) const = 0;
    virtual void close(data_stream *pStream) const = 0;

    virtual bool does_exist(const dynamic_string &id) const = 0;

    virtual uint64_t get_size(const dynamic_string &id) const = 0;

    virtual vogl::dynamic_string_array enumerate() const = 0;

    virtual vogl::dynamic_string copy_file(vogl_blob_manager &src_blob_manager, const vogl::dynamic_string &src_id, const vogl::dynamic_string &dst_id);

protected:
    uint32_t m_flags;

    bool is_readable() const
    {
        return (m_flags & cBMFReadable) != 0;
    }
    bool is_writable() const
    {
        return (m_flags & cBMFWritable) != 0;
    }
    bool is_read_only() const
    {
        return (m_flags & cBMFReadWrite) == cBMFReadable;
    }
    bool is_write_only() const
    {
        return (m_flags & cBMFReadWrite) == cBMFWritable;
    }

    bool init(uint32_t flags)
    {
        if ((flags & cBMFReadWrite) == 0)
            return false;
        m_flags = flags;
        return true;
    }

    bool m_initialized;
};

//----------------------------------------------------------------------------------------------------------------------
// class vogl_null_blob_manager
//----------------------------------------------------------------------------------------------------------------------
class vogl_null_blob_manager : public vogl_blob_manager
{
public:
    vogl_null_blob_manager()
        : vogl_blob_manager()
    {
    }

    virtual ~vogl_null_blob_manager()
    {
    }

    bool init(uint32_t flags)
    {
        vogl_blob_manager::init(flags);
        m_initialized = true;
        return true;
    }

    bool deinit()
    {
        return vogl_blob_manager::deinit();
    }

    virtual vogl_blob_manager_type_t get_type() const
    {
        return cBMTNull;
    }

    virtual vogl::dynamic_string add_buf_using_id(const void *pData, uint32_t size, const vogl::dynamic_string &id)
    {
        VOGL_NOTE_UNUSED(pData);
        VOGL_NOTE_UNUSED(size);
        return id;
    }

    virtual vogl::data_stream *open(const dynamic_string &id) const
    {
        VOGL_NOTE_UNUSED(id);
        return NULL;
    }

    virtual void close(vogl::data_stream *pStream) const
    {
        VOGL_NOTE_UNUSED(pStream);
    }

    virtual bool does_exist(const vogl::dynamic_string &id) const
    {
        VOGL_NOTE_UNUSED(id);
        return false;
    }

    virtual uint64_t get_size(const vogl::dynamic_string &id) const
    {
        VOGL_NOTE_UNUSED(id);
        return 0;
    }

    virtual vogl::dynamic_string_array enumerate() const
    {
        return vogl::dynamic_string_array();
    }
};

//----------------------------------------------------------------------------------------------------------------------
// class vogl_memory_blob_manager
//----------------------------------------------------------------------------------------------------------------------
class vogl_memory_blob_manager : public vogl_blob_manager
{
public:
    vogl_memory_blob_manager();
    virtual ~vogl_memory_blob_manager();

    bool init(uint32_t flags);

    virtual bool deinit();

    virtual vogl_blob_manager_type_t get_type() const
    {
        return cBMTMemory;
    }

    virtual vogl::dynamic_string add_buf_using_id(const void *pData, uint32_t size, const vogl::dynamic_string &id);

    virtual vogl::data_stream *open(const dynamic_string &id) const;
    virtual void close(vogl::data_stream *pStream) const;

    virtual bool does_exist(const vogl::dynamic_string &id) const;

    virtual uint64_t get_size(const vogl::dynamic_string &id) const;

    virtual vogl::dynamic_string_array enumerate() const;

    bool create_archive(mz_zip_archive &zip);
    bool read_archive(mz_zip_archive &zip);

private:
    struct blob
    {
        vogl::dynamic_string m_id;
        vogl::uint8_vec m_blob;
    };

    typedef vogl::map<vogl::dynamic_string, blob, vogl::dynamic_string_less_than_case_sensitive, vogl::dynamic_string_equal_to_case_sensitive> blob_map;
    blob_map m_blobs;
};

//----------------------------------------------------------------------------------------------------------------------
// class vogl_loose_file_blob_manager
//----------------------------------------------------------------------------------------------------------------------
class vogl_loose_file_blob_manager : public vogl_blob_manager
{
public:
    vogl_loose_file_blob_manager();
    virtual ~vogl_loose_file_blob_manager();

    bool init(uint32_t flags);
    bool init(uint32_t flags, const char *pPath);

    void set_path(const vogl::dynamic_string &path)
    {
        m_path = path;
    }
    const vogl::dynamic_string &get_path()
    {
        return m_path;
    }

    virtual bool deinit();

    virtual vogl_blob_manager_type_t get_type() const
    {
        return cBMTFile;
    }

    virtual vogl::dynamic_string add_buf_using_id(const void *pData, uint32_t size, const vogl::dynamic_string &id);

    virtual vogl::data_stream *open(const vogl::dynamic_string &id) const;
    virtual void close(vogl::data_stream *pStream) const;

    virtual bool does_exist(const vogl::dynamic_string &id) const;

    virtual uint64_t get_size(const vogl::dynamic_string &id) const;

    virtual vogl::dynamic_string_array enumerate() const;

private:
    vogl::dynamic_string get_filename(const vogl::dynamic_string &id) const;

    dynamic_string m_path;
};

//----------------------------------------------------------------------------------------------------------------------
// class vogl_archive_blob_manager
//----------------------------------------------------------------------------------------------------------------------
class vogl_archive_blob_manager : public vogl_blob_manager
{
public:
    vogl_archive_blob_manager();
    virtual ~vogl_archive_blob_manager();

    bool init_memory(uint32_t flags, const void *pZip_data, size_t size);

    bool init_heap(uint32_t flags);
    void *deinit_heap(size_t &size);

    bool init_file(uint32_t flags, const char *pFilename, uint64_t file_start_ofs = 0, uint64_t actual_archive_size = 0);

    // Same as init_file(), but creates a temporary archive. It's up to you to delete the archive.
    bool init_file_temp(uint32_t flags);

    bool init_cfile(uint32_t flags, FILE *pFile, uint64_t cur_size);

    const dynamic_string &get_archive_filename() const
    {
        return m_archive_filename;
    }

    uint64_t get_archive_size() const;
    bool write_archive_to_stream(vogl::data_stream &stream) const;

    // TODO: init_data_stream

    virtual bool deinit();

    virtual vogl_blob_manager_type_t get_type() const
    {
        return cBMTArchive;
    }

    virtual vogl::dynamic_string add_buf_using_id(const void *pData, uint32_t size, const vogl::dynamic_string &id);

    virtual vogl::data_stream *open(const vogl::dynamic_string &id) const;
    virtual void close(vogl::data_stream *pStream) const;

    virtual bool does_exist(const vogl::dynamic_string &id) const;

    virtual uint64_t get_size(const vogl::dynamic_string &id) const;

    virtual vogl::dynamic_string_array enumerate() const;

private:
    mutable mz_zip_archive m_zip;
    dynamic_string m_archive_filename;

    struct blob
    {
        vogl::dynamic_string m_id;
        uint32_t m_file_index;
        uint64_t m_size;

        blob()
        {
        }
        blob(const dynamic_string &id, uint32_t file_index, uint64_t size)
            : m_id(id), m_file_index(file_index), m_size(size)
        {
        }
    };

    typedef vogl::map<vogl::dynamic_string, blob, vogl::dynamic_string_less_than_case_sensitive, vogl::dynamic_string_equal_to_case_sensitive> blob_map;
    blob_map m_blobs;

    vogl::dynamic_string get_filename(const vogl::dynamic_string &id) const;
    bool populate_blob_map();
};

//----------------------------------------------------------------------------------------------------------------------
// class vogl_multi_blob_manager
//----------------------------------------------------------------------------------------------------------------------
class vogl_multi_blob_manager : public vogl_blob_manager
{
public:
    vogl_multi_blob_manager();

    virtual ~vogl_multi_blob_manager();

    bool init(uint32_t flags);

    void add_blob_manager(vogl_blob_manager *pBlob_manager);

    void remove_blob_manager(vogl_blob_manager *pBlob_manager);

    typedef vogl::vector<vogl_blob_manager *> vogl_blob_manager_ptr_vec;

    const vogl_blob_manager_ptr_vec &get_blob_managers() const
    {
        return m_blob_managers;
    }
    vogl_blob_manager_ptr_vec &get_blob_managers()
    {
        return m_blob_managers;
    }

    virtual bool deinit();

    virtual vogl_blob_manager_type_t get_type() const
    {
        return cBMTMulti;
    }

    virtual vogl::dynamic_string add_buf_using_id(const void *pData, uint32_t size, const vogl::dynamic_string &id);

    virtual vogl::data_stream *open(const dynamic_string &id) const;

    virtual void close(vogl::data_stream *pStream) const;

    virtual bool does_exist(const vogl::dynamic_string &id) const;

    virtual uint64_t get_size(const vogl::dynamic_string &id) const;

    virtual vogl::dynamic_string_array enumerate() const;

private:
    vogl_blob_manager_ptr_vec m_blob_managers;
};

#endif // VOGL_BLOB_MANAGER_H
