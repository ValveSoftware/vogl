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

// File: vogl_blob_manager.cpp
#include "vogl_common.h"
#include "vogl_blob_manager.h"
#include "vogl_buffer_stream.h"
#include "vogl_cfile_stream.h"
#include "vogl_file_utils.h"
#include "vogl_find_files.h"
#include "vogl_hash.h"

using namespace vogl;

//----------------------------------------------------------------------------------------------------------------------
// vogl_blob_manager
//----------------------------------------------------------------------------------------------------------------------
vogl_blob_manager::vogl_blob_manager()
    : m_flags(0),
      m_initialized(false)
{
    VOGL_FUNC_TRACER
}

vogl_blob_manager::~vogl_blob_manager()
{
    VOGL_FUNC_TRACER

    deinit();
}

dynamic_string vogl_blob_manager::compute_unique_id(const void *pData, uint32_t size, const dynamic_string &prefix, const dynamic_string &ext, const uint64_t *pCRC64) const
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(!prefix.contains('['));
    VOGL_ASSERT(!prefix.contains(']'));

    uint64_t crc64 = pCRC64 ? *pCRC64 : calc_crc64(CRC64_INIT, static_cast<const uint8_t *>(pData), size);

    dynamic_string actual_ext(ext);
    if ((actual_ext.get_len() > 1) && (actual_ext[0] == '.'))
        actual_ext.right(1);

    if (prefix.get_len())
        return dynamic_string(cVarArg, "[%s]_%" PRIX64 "_%u.radblob.%s", prefix.get_ptr(), crc64, size, actual_ext.get_len() ? actual_ext.get_ptr() : "raw");
    else
        return dynamic_string(cVarArg, "%" PRIX64 "_%u.radblob.%s", crc64, size, actual_ext.get_len() ? actual_ext.get_ptr() : "raw");
}

dynamic_string vogl_blob_manager::get_prefix(const dynamic_string &id) const
{
    VOGL_FUNC_TRACER

    if (!id.is_empty() && id.begins_with("["))
    {
        dynamic_string prefix(id);
        int e = prefix.find_right(']');
        return prefix.mid(1, e - 1);
    }
    return "";
}

dynamic_string vogl_blob_manager::get_extension(const dynamic_string &id) const
{
    VOGL_FUNC_TRACER

    dynamic_string extension(id);
    file_utils::get_extension(extension);
    return extension;
}

bool vogl_blob_manager::get(const dynamic_string &id, uint8_vec &data) const
{
    VOGL_FUNC_TRACER

    if (!is_initialized())
    {
        VOGL_ASSERT(0);
        return false;
    }

    data_stream *pStream = open(id);
    if (!pStream)
    {
        data.resize(0);
        vogl_error_printf("Failed finding blob ID %s\n", id.get_ptr());
        return false;
    }

    // TODO
    if (pStream->get_size() > static_cast<uint64_t>(cINT32_MAX))
    {
        close(pStream);

        VOGL_ASSERT_ALWAYS;

        vogl_error_printf("Blob is too large: blob ID %s, size %" PRIu64 "\n", id.get_ptr(), pStream->get_size());

        return false;
    }

    uint32_t size = static_cast<uint32_t>(pStream->get_size());

    if (!data.try_resize(size))
    {
        VOGL_ASSERT_ALWAYS;
        vogl_error_printf("Out of memory while trying to read blob ID %s, size %u\n", id.get_ptr(), size);
        return false;
    }

    if (size)
    {
        if (pStream->read(data.get_ptr(), size) != size)
        {
            close(pStream);

            data.clear();

            vogl_error_printf("Failed reading blob ID %s, size %u\n", id.get_ptr(), size);

            return false;
        }
    }

    close(pStream);
    return true;
}

bool vogl_blob_manager::populate(const vogl_blob_manager &other)
{
    VOGL_FUNC_TRACER

    if (!is_initialized() || !other.is_initialized())
    {
        VOGL_ASSERT(0);
        return false;
    }

    dynamic_string_array ids(other.enumerate());

    bool success = true;
    for (uint32_t i = 0; i < ids.size(); i++)
    {
        const dynamic_string &id = ids[i];

        data_stream *pStream = other.open(id);
        if (!pStream)
        {
            success = false;
            continue;
        }

        dynamic_string new_id(add_stream_using_id(*pStream, id));
        if (new_id.is_empty())
            success = false;

        other.close(pStream);
    }

    return success;
}

dynamic_string vogl_blob_manager::add_buf_compute_unique_id(const void *pData, uint32_t size, const vogl::dynamic_string &prefix, const dynamic_string &ext, const uint64_t *pCRC64)
{
    VOGL_FUNC_TRACER

    if (!is_initialized() || !is_writable())
        return "";

    dynamic_string id(compute_unique_id(pData, size, prefix, ext, pCRC64));

    return add_buf_using_id(pData, size, id);
}

dynamic_string vogl_blob_manager::add_stream_compute_unique_id(data_stream &stream, const dynamic_string &prefix, const dynamic_string &ext, const uint64_t *pCRC64)
{
    VOGL_FUNC_TRACER

    if (!is_initialized() || !is_writable())
    {
        VOGL_ASSERT(0);
        return "";
    }

    // TODO
    if (stream.get_size() > static_cast<uint64_t>(cUINT32_MAX))
    {
        VOGL_VERIFY(0);
        return "";
    }

    uint64_t size64 = stream.get_size();

    // TODO
    if ((size64 > static_cast<uint64_t>(VOGL_MAX_POSSIBLE_HEAP_BLOCK_SIZE)) || (size64 > cUINT32_MAX))
    {
        VOGL_VERIFY(0);
        return "";
    }

    uint32_t size = static_cast<uint32_t>(size64);

    void *pData = vogl_malloc(static_cast<size_t>(size));
    if (!pData)
        return "";

    if (!stream.seek(0, false))
    {
        vogl_free(pData);
        return "";
    }

    if (stream.read64(pData, size) != size)
    {
        vogl_free(pData);
        return "";
    }

    dynamic_string id(compute_unique_id(pData, size, prefix, ext, pCRC64));

    dynamic_string actual_id(add_buf_using_id(pData, size, id));

    vogl_free(pData);
    return actual_id;
}

dynamic_string vogl_blob_manager::add_stream_using_id(data_stream &stream, const dynamic_string &id)
{
    VOGL_FUNC_TRACER

    if (!is_initialized() || !is_writable())
    {
        VOGL_ASSERT(0);
        return "";
    }

    uint64_t size64 = stream.get_size();

    // TODO
    if ((size64 > static_cast<uint64_t>(VOGL_MAX_POSSIBLE_HEAP_BLOCK_SIZE)) || (size64 > cUINT32_MAX))
    {
        VOGL_VERIFY(0);
        return "";
    }

    uint32_t size = static_cast<uint32_t>(size64);

    void *pData = vogl_malloc(static_cast<size_t>(size));
    if (!pData)
        return "";

    if (!stream.seek(0, false))
    {
        vogl_free(pData);
        return "";
    }

    if (stream.read64(pData, size) != size)
    {
        vogl_free(pData);
        return "";
    }

    dynamic_string actual_id(add_buf_using_id(pData, size, id));

    vogl_free(pData);
    return actual_id;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_blob_manager::copy_file
//----------------------------------------------------------------------------------------------------------------------
vogl::dynamic_string vogl_blob_manager::copy_file(vogl_blob_manager &src_blob_manager, const vogl::dynamic_string &src_id, const vogl::dynamic_string &dst_id)
{
    uint8_vec data;
    if (!src_blob_manager.get(src_id, data))
        return "";

    return add_buf_using_id(data.get_ptr(), data.size(), dst_id);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_memory_blob_manager
//----------------------------------------------------------------------------------------------------------------------
vogl_memory_blob_manager::vogl_memory_blob_manager()
    : vogl_blob_manager()
{
    VOGL_FUNC_TRACER
}

vogl_memory_blob_manager::~vogl_memory_blob_manager()
{
    VOGL_FUNC_TRACER

    deinit();
}

bool vogl_memory_blob_manager::init(uint32_t flags)
{
    VOGL_FUNC_TRACER

    deinit();

    if (!vogl_blob_manager::init(flags))
        return false;

    m_initialized = true;
    return true;
}

bool vogl_memory_blob_manager::deinit()
{
    VOGL_FUNC_TRACER

    m_blobs.clear();
    return vogl_blob_manager::deinit();
}

dynamic_string vogl_memory_blob_manager::add_buf_using_id(const void *pData, uint32_t size, const dynamic_string &id)
{
    VOGL_FUNC_TRACER

    if (!is_initialized() || !is_writable())
    {
        VOGL_ASSERT(0);
        return "";
    }

    dynamic_string actual_id(id);
    if (actual_id.is_empty())
        actual_id = compute_unique_id(pData, size);

    blob_map::insert_result insert_res(m_blobs.insert(actual_id));
    if (!insert_res.second)
        return (insert_res.first)->first;

    blob &new_blob = (insert_res.first)->second;
    new_blob.m_id = actual_id;
    new_blob.m_blob.append(static_cast<const uint8_t *>(pData), static_cast<uint32_t>(size));

    return actual_id;
}

data_stream *vogl_memory_blob_manager::open(const dynamic_string &id) const
{
    VOGL_FUNC_TRACER

    if (!is_initialized() || !is_readable())
    {
        VOGL_ASSERT(0);
        return NULL;
    }

    const blob *pBlob = m_blobs.find_value(id);
    if (!pBlob)
        return NULL;

    return vogl_new(buffer_stream, pBlob->m_blob.get_ptr(), pBlob->m_blob.size());
}

void vogl_memory_blob_manager::close(data_stream *pStream) const
{
    VOGL_FUNC_TRACER

    vogl_delete(pStream);
}

bool vogl_memory_blob_manager::does_exist(const dynamic_string &id) const
{
    VOGL_FUNC_TRACER

    return m_blobs.contains(id);
}

uint64_t vogl_memory_blob_manager::get_size(const dynamic_string &id) const
{
    VOGL_FUNC_TRACER

    const blob *pBlob = m_blobs.find_value(id);
    return pBlob ? pBlob->m_blob.size() : 0;
}

dynamic_string_array vogl_memory_blob_manager::enumerate() const
{
    VOGL_FUNC_TRACER

    if (!is_initialized())
    {
        VOGL_ASSERT(0);
        return dynamic_string_array();
    }

    dynamic_string_array ids;
    return m_blobs.get_keys(ids);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_file_blob_manager
//----------------------------------------------------------------------------------------------------------------------

vogl_loose_file_blob_manager::vogl_loose_file_blob_manager()
    : vogl_blob_manager()
{
    VOGL_FUNC_TRACER
}

vogl_loose_file_blob_manager::~vogl_loose_file_blob_manager()
{
    VOGL_FUNC_TRACER

    deinit();
}

bool vogl_loose_file_blob_manager::init(uint32_t flags)
{
    VOGL_FUNC_TRACER

    deinit();

    if (!vogl_blob_manager::init(flags))
        return false;
    m_initialized = true;
    return true;
}

bool vogl_loose_file_blob_manager::init(uint32_t flags, const char *pPath)
{
    VOGL_FUNC_TRACER

    deinit();

    if (!vogl_blob_manager::init(flags))
        return false;
    m_path = pPath;
    m_initialized = true;
    return true;
}

bool vogl_loose_file_blob_manager::deinit()
{
    VOGL_FUNC_TRACER

    return vogl_blob_manager::deinit();
}

dynamic_string vogl_loose_file_blob_manager::add_buf_using_id(const void *pData, uint32_t size, const dynamic_string &id)
{
    VOGL_FUNC_TRACER

    if (!is_writable())
    {
        VOGL_ASSERT(0);
        return "";
    }

    dynamic_string actual_id(id);
    if (actual_id.is_empty())
        actual_id = compute_unique_id(pData, size);

    dynamic_string filename(get_filename(actual_id));

    if (does_exist(filename))
    {
        uint64_t cur_size = get_size(actual_id);
        if (cur_size != size)
            vogl_error_printf("Not overwrite already existing blob %s desired size %u, but it has the wrong size on disk (%" PRIu64 " bytes)!\n", filename.get_ptr(), size, cur_size);
        else
            vogl_message_printf("Not overwriting already existing blob %s size %u\n", filename.get_ptr(), size);
        return actual_id;
    }

    cfile_stream out_file(filename.get_ptr(), cDataStreamWritable);
    if (!out_file.is_opened())
    {
        vogl_error_printf("Failed creating file \"%s\"!\n", filename.get_ptr());
        return "";
    }

    if (out_file.write(pData, size) != size)
    {
        VOGL_VERIFY(0);

        out_file.close();
        file_utils::delete_file(filename.get_ptr());

        vogl_error_printf("Failed writing to file \"%s\"!\n", filename.get_ptr());

        return "";
    }

    if (!out_file.close())
    {
        VOGL_VERIFY(0);

        file_utils::delete_file(filename.get_ptr());

        vogl_error_printf("Failed writing to file \"%s\"!\n", filename.get_ptr());

        return "";
    }

    return actual_id;
}

data_stream *vogl_loose_file_blob_manager::open(const dynamic_string &id) const
{
    VOGL_FUNC_TRACER

    if (!is_readable())
    {
        VOGL_ASSERT(0);
        return NULL;
    }

    cfile_stream *pStream = vogl_new(cfile_stream, get_filename(id).get_ptr());
    if (!pStream->is_opened())
    {
        vogl_delete(pStream);
        return NULL;
    }
    return pStream;
}

void vogl_loose_file_blob_manager::close(data_stream *pStream) const
{
    VOGL_FUNC_TRACER

    vogl_delete(pStream);
}

bool vogl_loose_file_blob_manager::does_exist(const dynamic_string &id) const
{
    VOGL_FUNC_TRACER

    if (!is_initialized())
    {
        VOGL_ASSERT(0);
        return false;
    }

    return file_utils::does_file_exist(get_filename(id).get_ptr());
}

uint64_t vogl_loose_file_blob_manager::get_size(const dynamic_string &id) const
{
    VOGL_FUNC_TRACER

    if (!is_initialized())
    {
        VOGL_ASSERT(0);
        return false;
    }

    uint32_t file_size;
    file_utils::get_file_size(get_filename(id).get_ptr(), file_size);
    return file_size;
}

dynamic_string vogl_loose_file_blob_manager::get_filename(const dynamic_string &id) const
{
    VOGL_FUNC_TRACER

    if (!is_initialized())
    {
        VOGL_ASSERT(0);
        return "";
    }

    dynamic_string file_path;
    file_utils::combine_path(file_path, m_path.get_ptr(), id.get_ptr());
    return file_path;
}

dynamic_string_array vogl_loose_file_blob_manager::enumerate() const
{
    VOGL_FUNC_TRACER

    dynamic_string_array files;

    if (!is_initialized())
    {
        VOGL_ASSERT(0);
        return files;
    }

    dynamic_string find_path(get_filename("*.radblob.*"));

    find_files finder;

    bool success = finder.find(find_path.get_ptr());
    if (success)
    {
        const find_files::file_desc_vec &found_files = finder.get_files();

        for (uint32_t i = 0; i < found_files.size(); i++)
            files.push_back(found_files[i].m_name);
    }

    return files;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_archive_blob_manager
//----------------------------------------------------------------------------------------------------------------------
vogl_archive_blob_manager::vogl_archive_blob_manager()
    : vogl_blob_manager()
{
    VOGL_FUNC_TRACER

    mz_zip_zero_struct(&m_zip);
}

vogl_archive_blob_manager::~vogl_archive_blob_manager()
{
    VOGL_FUNC_TRACER

    deinit();
}

bool vogl_archive_blob_manager::init_memory(uint32_t flags, const void *pZip_data, size_t size)
{
    VOGL_FUNC_TRACER

    deinit();

    if (!vogl_blob_manager::init(flags))
        return false;

    if (is_writable() || !is_readable())
    {
        deinit();
        return false;
    }

    if (!mz_zip_reader_init_mem(&m_zip, pZip_data, size, 0))
    {
        mz_zip_error mz_err = mz_zip_get_last_error(&m_zip);
        vogl_error_printf("mz_zip_reader_init_mem() failed initing memory size %" PRIu64 ", error 0x%X (%s)\n", cast_val_to_uint64(size), mz_err, mz_zip_get_error_string(mz_err));

        deinit();
        return false;
    }

    populate_blob_map();

    m_initialized = true;

    return true;
}

bool vogl_archive_blob_manager::init_heap(uint32_t flags)
{
    VOGL_FUNC_TRACER

    deinit();

    if (!vogl_blob_manager::init(flags))
        return false;

    if (!is_writable())
    {
        deinit();
        return false;
    }

    if (!mz_zip_writer_init_heap(&m_zip, 0, 1024, MZ_ZIP_FLAG_WRITE_ALLOW_READING | MZ_ZIP_FLAG_WRITE_ZIP64))
    {
        mz_zip_error mz_err = mz_zip_get_last_error(&m_zip);
        vogl_error_printf("mz_zip_writer_init_heap() failed initing heap with flags 0x%x, error 0x%X (%s)\n", flags, mz_err, mz_zip_get_error_string(mz_err));

        deinit();
        return false;
    }

    m_initialized = true;

    return true;
}

void *vogl_archive_blob_manager::deinit_heap(size_t &size)
{
    VOGL_FUNC_TRACER

    size = 0;

    if ((mz_zip_get_mode(&m_zip) == MZ_ZIP_MODE_INVALID) || (mz_zip_get_type(&m_zip) != MZ_ZIP_TYPE_HEAP))
        return NULL;

    void *pBuf = NULL;
    if (!mz_zip_writer_finalize_heap_archive(&m_zip, &pBuf, &size))
    {
        mz_zip_error mz_err = mz_zip_get_last_error(&m_zip);
        vogl_error_printf("mz_zip_writer_finalize_heap_archive() failed, error 0x%X (%s)\n", mz_err, mz_zip_get_error_string(mz_err));

        pBuf = NULL;
    }

    deinit();

    return pBuf;
}

bool vogl_archive_blob_manager::init_file(uint32_t flags, const char *pFilename, uint64_t file_start_ofs, uint64_t actual_archive_size)
{
    VOGL_FUNC_TRACER

    deinit();

    if (!vogl_blob_manager::init(flags))
        return false;

    m_archive_filename = pFilename;

    // read existing
    // create new
    // open existing and read/append

    if (is_writable() && (flags & (cBMFOpenExisting | cBMFOpenExistingOrCreateNew)))
    {
        bool does_file_exist = file_utils::does_file_exist(pFilename);

        // open existing for read/write
        if (does_file_exist)
        {
            if (!mz_zip_reader_init_file(&m_zip, pFilename, 0, file_start_ofs, actual_archive_size))
            {
                mz_zip_error mz_err = mz_zip_get_last_error(&m_zip);
                vogl_error_printf("mz_zip_reader_init_file() failed with filename \"%s\", error 0x%X (%s)\n", pFilename, mz_err, mz_zip_get_error_string(mz_err));

                deinit();
                return false;
            }

            if (!mz_zip_writer_init_from_reader(&m_zip, pFilename, MZ_ZIP_FLAG_WRITE_ZIP64))
            {
                mz_zip_error mz_err = mz_zip_get_last_error(&m_zip);
                vogl_error_printf("mz_zip_writer_init_from_reader() failed with filename \"%s\", error 0x%X (%s)\n", pFilename, mz_err, mz_zip_get_error_string(mz_err));

                deinit();
                return false;
            }
        }
        else
        {
            // File does not exist - check if the caller allows us to create a new archive.
            if ((flags & cBMFOpenExistingOrCreateNew) == 0)
            {
                deinit();
                return false;
            }

            // Create new archive, write only or read/write.
            if (!mz_zip_writer_init_file(&m_zip, pFilename, file_start_ofs, (is_readable() ? MZ_ZIP_FLAG_WRITE_ALLOW_READING : 0) | MZ_ZIP_FLAG_WRITE_ZIP64))
            {
                mz_zip_error mz_err = mz_zip_get_last_error(&m_zip);
                vogl_error_printf("mz_zip_writer_init_file() failed with filename \"%s\", error 0x%X (%s)\n", pFilename, mz_err, mz_zip_get_error_string(mz_err));

                deinit();
                return false;
            }
        }
    }
    else if (is_read_only())
    {
        // open existing for reading
        if (flags & cBMFOpenExistingOrCreateNew)
        {
            deinit();
            return false;
        }

        if (!mz_zip_reader_init_file(&m_zip, pFilename, 0, file_start_ofs, actual_archive_size))
        {
            mz_zip_error mz_err = mz_zip_get_last_error(&m_zip);
            vogl_error_printf("mz_zip_reader_init_file() failed with filename \"%s\", error 0x%X (%s)\n", pFilename, mz_err, mz_zip_get_error_string(mz_err));

            deinit();
            return false;
        }
    }
    else
    {
        VOGL_ASSERT(!actual_archive_size);

        // create new archive, read/write or write only
        if (!mz_zip_writer_init_file(&m_zip, pFilename, file_start_ofs, (is_readable() ? MZ_ZIP_FLAG_WRITE_ALLOW_READING : 0) | MZ_ZIP_FLAG_WRITE_ZIP64))
        {
            mz_zip_error mz_err = mz_zip_get_last_error(&m_zip);
            vogl_error_printf("mz_zip_writer_init_file() failed with filename \"%s\", error 0x%X (%s)\n", pFilename, mz_err, mz_zip_get_error_string(mz_err));

            deinit();
            return false;
        }
    }

    if (!populate_blob_map())
    {
        deinit();
        return false;
    }

    m_initialized = true;

    return true;
}

bool vogl_archive_blob_manager::init_file_temp(uint32_t flags)
{
    VOGL_FUNC_TRACER

    dynamic_string temp_filename(file_utils::generate_temp_filename("voglblob"));
    return init_file(flags, temp_filename.get_ptr());
}

bool vogl_archive_blob_manager::init_cfile(uint32_t flags, FILE *pFile, uint64_t cur_size)
{
    VOGL_FUNC_TRACER

    deinit();

    if (!vogl_blob_manager::init(flags))
        return false;

    m_archive_filename = "<cfile>";

    if (is_writable())
    {
        // Writable blob manager - see if there's any existing archive data
        if (cur_size)
        {
            if (!mz_zip_reader_init_cfile(&m_zip, pFile, cur_size, 0))
            {
                mz_zip_error mz_err = mz_zip_get_last_error(&m_zip);
                vogl_error_printf("mz_zip_reader_init_cfile() failed, error 0x%X (%s)\n", mz_err, mz_zip_get_error_string(mz_err));

                deinit();
                return false;
            }

            if (!mz_zip_writer_init_from_reader(&m_zip, NULL, MZ_ZIP_FLAG_WRITE_ZIP64))
            {
                mz_zip_error mz_err = mz_zip_get_last_error(&m_zip);
                vogl_error_printf("mz_zip_writer_init_from_reader() failed, error 0x%X (%s)\n", mz_err, mz_zip_get_error_string(mz_err));

                deinit();
                return false;
            }
        }
        else
        {
            // create new archive, write only or read/write
            if (!mz_zip_writer_init_cfile(&m_zip, pFile, (is_readable() ? MZ_ZIP_FLAG_WRITE_ALLOW_READING : 0) | MZ_ZIP_FLAG_WRITE_ZIP64))
            {
                mz_zip_error mz_err = mz_zip_get_last_error(&m_zip);
                vogl_error_printf("mz_zip_writer_init_cfile() failed, error 0x%X (%s)\n", mz_err, mz_zip_get_error_string(mz_err));

                deinit();
                return false;
            }
        }
    }
    else if (is_read_only())
    {
        // Open existing read-only archive
        if (!mz_zip_reader_init_cfile(&m_zip, pFile, cur_size, 0))
        {
            mz_zip_error mz_err = mz_zip_get_last_error(&m_zip);
            vogl_error_printf("mz_zip_reader_init_cfile() failed, error 0x%X (%s)\n", mz_err, mz_zip_get_error_string(mz_err));

            deinit();
            return false;
        }
    }
    else
    {
        // create new archive, read/write or write only
        if (cur_size)
        {
            deinit();
            return false;
        }

        if (!mz_zip_writer_init_cfile(&m_zip, pFile, (is_readable() ? MZ_ZIP_FLAG_WRITE_ALLOW_READING : 0) | MZ_ZIP_FLAG_WRITE_ZIP64))
        {
            mz_zip_error mz_err = mz_zip_get_last_error(&m_zip);
            vogl_error_printf("mz_zip_writer_init_cfile() failed, error 0x%X (%s)\n", mz_err, mz_zip_get_error_string(mz_err));

            deinit();
            return false;
        }
    }

    if (!populate_blob_map())
    {
        deinit();
        return false;
    }

    return true;
}

bool vogl_archive_blob_manager::populate_blob_map()
{
    VOGL_FUNC_TRACER

    m_blobs.clear();

    for (uint32_t file_index = 0; file_index < mz_zip_get_num_files(&m_zip); file_index++)
    {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_file_stat(&m_zip, file_index, &stat))
        {
            mz_zip_error mz_err = mz_zip_get_last_error(&m_zip);
            vogl_error_printf("mz_zip_file_stat() failed, error 0x%X (%s)\n", mz_err, mz_zip_get_error_string(mz_err));

            return false;
        }

        if (!m_blobs.insert(stat.m_filename, blob(stat.m_filename, file_index, stat.m_uncomp_size)).second)
        {
            vogl_warning_printf("Duplicate file %s in blob archive %s\n", stat.m_filename, m_archive_filename.get_ptr());
        }
    }

    return true;
}

bool vogl_archive_blob_manager::deinit()
{
    VOGL_FUNC_TRACER

    bool status = true;
    VOGL_NOTE_UNUSED(status);

    if (mz_zip_get_mode(&m_zip) != MZ_ZIP_MODE_INVALID)
    {
        if ((mz_zip_get_type(&m_zip) == MZ_ZIP_TYPE_FILE) && (mz_zip_get_mode(&m_zip) == MZ_ZIP_MODE_WRITING))
        {
            if (!mz_zip_writer_finalize_archive(&m_zip))
            {
                mz_zip_error mz_err = mz_zip_get_last_error(&m_zip);
                vogl_error_printf("mz_zip_writer_finalize_archive() failed, error 0x%X (%s)\n", mz_err, mz_zip_get_error_string(mz_err));

                status = false;
            }
        }

        if (!mz_zip_end(&m_zip))
        {
            mz_zip_error mz_err = mz_zip_get_last_error(&m_zip);
            vogl_error_printf("mz_zip_end() failed, error 0x%X (%s)\n", mz_err, mz_zip_get_error_string(mz_err));

            status = false;
        }
    }

    mz_zip_zero_struct(&m_zip);

    m_archive_filename.clear();

    m_blobs.clear();

    return vogl_blob_manager::deinit();
}

vogl::dynamic_string vogl_archive_blob_manager::add_buf_using_id(const void *pData, uint32_t size, const vogl::dynamic_string &id)
{
    VOGL_FUNC_TRACER

    if (!is_initialized() || !is_writable())
    {
        VOGL_ASSERT(0);
        return "";
    }

    if (mz_zip_get_mode(&m_zip) != MZ_ZIP_MODE_WRITING)
        return "";

    dynamic_string actual_id(id);
    if (actual_id.is_empty())
        actual_id = compute_unique_id(pData, size);

    // We don't support overwriting files already in the archive - it's up to the caller to not try adding redundant files into the archive.
    // We could support orphaning the previous copy of the file and updating the archive to point to the latest version, though.
    if (m_blobs.contains(actual_id))
    {
        vogl_debug_printf("Archive already contains blob id \"%s\"! Not replacing file.\n", actual_id.get_ptr());
        return actual_id;
    }

    uint32_t file_index = mz_zip_get_num_files(&m_zip);

    // TODO: Allow caller to control whether files are compressed
    if (!mz_zip_writer_add_mem(&m_zip, actual_id.get_ptr(), pData, size, MZ_BEST_SPEED))
    //if (!mz_zip_writer_add_mem(&m_zip, actual_id.get_ptr(), pData, size, 0))
    {
        mz_zip_error mz_err = mz_zip_get_last_error(&m_zip);
        vogl_error_printf("mz_zip_writer_add_mem() failed adding blob \"%s\" size %u, error 0x%X (%s)\n", id.get_ptr(), size, mz_err, mz_zip_get_error_string(mz_err));

        return "";
    }

    bool success = m_blobs.insert(actual_id, blob(actual_id, file_index, size)).second;
    VOGL_NOTE_UNUSED(success);
    VOGL_ASSERT(success);

    return actual_id;
}

vogl::data_stream *vogl_archive_blob_manager::open(const vogl::dynamic_string &id) const
{
    VOGL_FUNC_TRACER

    if (!is_initialized() || !is_readable())
    {
        VOGL_ASSERT(0);
        return NULL;
    }

    blob_map::const_iterator it = m_blobs.find(id);
    if (it == m_blobs.end())
        return NULL;

    // TODO: Add some sort of streaming decompression support to miniz and this class.

    mz_zip_clear_last_error(&m_zip);

    size_t size;
    void *pBuf = mz_zip_extract_to_heap(&m_zip, it->second.m_file_index, &size, 0);
    if (!pBuf)
    {
        mz_zip_error mz_err = mz_zip_get_last_error(&m_zip);
        vogl_error_printf("mz_zip_extract_to_heap() failed opening blob \"%s\", error 0x%X (%s)\n", id.get_ptr(), mz_err, mz_zip_get_error_string(mz_err));

        return NULL;
    }

    VOGL_VERIFY(size == it->second.m_size);

    return vogl_new(vogl::buffer_stream, pBuf, size);
}

void vogl_archive_blob_manager::close(vogl::data_stream *pStream) const
{
    VOGL_FUNC_TRACER

    if (pStream)
    {
        void *pBuf = const_cast<void *>(pStream->get_ptr());

        vogl_delete(pStream);

        mz_free(pBuf);
    }
}

bool vogl_archive_blob_manager::does_exist(const vogl::dynamic_string &id) const
{
    VOGL_FUNC_TRACER

    if (!is_initialized())
    {
        VOGL_ASSERT(0);
        return false;
    }

    return m_blobs.contains(id);
}

uint64_t vogl_archive_blob_manager::get_size(const vogl::dynamic_string &id) const
{
    VOGL_FUNC_TRACER

    if (!is_initialized())
    {
        VOGL_ASSERT(0);
        return false;
    }

    blob_map::const_iterator it = m_blobs.find(id);
    if (it == m_blobs.end())
        return 0;

    return it->second.m_size;
}

vogl::dynamic_string_array vogl_archive_blob_manager::enumerate() const
{
    VOGL_FUNC_TRACER

    if (!is_initialized())
    {
        VOGL_ASSERT(0);
        return vogl::dynamic_string_array();
    }

    vogl::dynamic_string_array result(m_blobs.size());

    uint32_t index = 0;
    for (blob_map::const_iterator it = m_blobs.begin(); it != m_blobs.end(); ++it)
        result[index++] = it->first;

    return result;
}

uint64_t vogl_archive_blob_manager::get_archive_size() const
{
    VOGL_FUNC_TRACER

    if (!is_initialized())
        return false;

    return mz_zip_get_archive_size(&m_zip);
}

bool vogl_archive_blob_manager::write_archive_to_stream(vogl::data_stream &stream) const
{
    VOGL_FUNC_TRACER

    if (!is_initialized())
        return false;

    uint8_vec buf(64 * 1024);

    uint64_t bytes_remaining = mz_zip_get_archive_size(&m_zip);
    mz_uint64 src_file_ofs = 0;

    while (bytes_remaining)
    {
        size_t n = static_cast<size_t>(math::minimum<uint64_t>(bytes_remaining, buf.size()));

        if (mz_zip_read_archive_data(&m_zip, src_file_ofs, buf.get_ptr(), n) != n)
        {
            mz_zip_error mz_err = mz_zip_get_last_error(&m_zip);
            vogl_error_printf("mz_zip_read_archive_data() failed, error 0x%X (%s)\n", mz_err, mz_zip_get_error_string(mz_err));

            return false;
        }

        if (stream.write(buf.get_ptr(), static_cast<uint32_t>(n)) != n)
            return false;

        src_file_ofs += n;
        bytes_remaining -= n;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_multi_blob_manager
//----------------------------------------------------------------------------------------------------------------------
vogl_multi_blob_manager::vogl_multi_blob_manager()
{
    VOGL_FUNC_TRACER
}

vogl_multi_blob_manager::~vogl_multi_blob_manager()
{
    VOGL_FUNC_TRACER
}

bool vogl_multi_blob_manager::init(uint32_t flags)
{
    VOGL_FUNC_TRACER

    deinit();

    if (flags & (cBMFWritable | cBMFOpenExistingOrCreateNew))
        return false;

    if (!vogl_blob_manager::init(flags))
        return false;

    m_initialized = true;

    return true;
}

void vogl_multi_blob_manager::add_blob_manager(vogl_blob_manager *pBlob_manager)
{
    VOGL_FUNC_TRACER

    if (!is_initialized())
    {
        VOGL_ASSERT(0);
        return;
    }

    m_blob_managers.push_back(pBlob_manager);
}

void vogl_multi_blob_manager::remove_blob_manager(vogl_blob_manager *pBlob_manager)
{
    VOGL_FUNC_TRACER

    if (!is_initialized())
    {
        VOGL_ASSERT(0);
        return;
    }

    int index = m_blob_managers.find(pBlob_manager);
    if (index >= 0)
        m_blob_managers.erase(index);
}

bool vogl_multi_blob_manager::deinit()
{
    VOGL_FUNC_TRACER

    m_blob_managers.clear();

    return vogl_blob_manager::deinit();
}

vogl::dynamic_string vogl_multi_blob_manager::add_buf_using_id(const void *pData, uint32_t size, const vogl::dynamic_string &id)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(0);
    VOGL_NOTE_UNUSED(pData);
    VOGL_NOTE_UNUSED(size);
    VOGL_NOTE_UNUSED(id);
    return "";
}

vogl::data_stream *vogl_multi_blob_manager::open(const dynamic_string &id) const
{
    VOGL_FUNC_TRACER

    if (!is_initialized())
    {
        VOGL_ASSERT(0);
        return NULL;
    }

    for (uint32_t i = 0; i < m_blob_managers.size(); i++)
    {
        if (!m_blob_managers[i]->is_initialized())
            continue;

        vogl::data_stream *pStream = m_blob_managers[i]->open(id);
        if (pStream)
        {
            VOGL_ASSERT(!pStream->get_user_data());
            pStream->set_user_data(m_blob_managers[i]);
            return pStream;
        }
    }

    return NULL;
}

void vogl_multi_blob_manager::close(vogl::data_stream *pStream) const
{
    VOGL_FUNC_TRACER

    if (pStream)
    {
        vogl_blob_manager *pBlob_manager = static_cast<vogl_blob_manager *>(pStream->get_user_data());
        VOGL_ASSERT(pBlob_manager);

        if (pBlob_manager)
        {
            pStream->set_user_data(NULL);
            pBlob_manager->close(pStream);
        }
    }
}

bool vogl_multi_blob_manager::does_exist(const vogl::dynamic_string &id) const
{
    VOGL_FUNC_TRACER

    for (uint32_t i = 0; i < m_blob_managers.size(); i++)
    {
        if (!m_blob_managers[i]->is_initialized())
            continue;

        if (m_blob_managers[i]->does_exist(id))
            return true;
    }
    return false;
}

uint64_t vogl_multi_blob_manager::get_size(const vogl::dynamic_string &id) const
{
    VOGL_FUNC_TRACER

    for (uint32_t i = 0; i < m_blob_managers.size(); i++)
    {
        if (!m_blob_managers[i]->is_initialized())
            continue;

        if (m_blob_managers[i]->does_exist(id))
            return m_blob_managers[i]->get_size(id);
    }
    return 0;
}

vogl::dynamic_string_array vogl_multi_blob_manager::enumerate() const
{
    VOGL_FUNC_TRACER

    vogl::dynamic_string_array all_files;

    for (uint32_t i = 0; i < m_blob_managers.size(); i++)
    {
        if (!m_blob_managers[i]->is_initialized())
            continue;

        vogl::dynamic_string_array files(m_blob_managers[i]->enumerate());
        all_files.append(files);
    }

    all_files.sort();
    all_files.unique();

    return all_files;
}
