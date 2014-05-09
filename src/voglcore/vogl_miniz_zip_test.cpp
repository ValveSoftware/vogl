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

// File: vogl_miniz_zip_test.cpp
#include "vogl_miniz_zip.h"
#include "vogl_core.h"
#include "vogl_cfile_stream.h"
#include "vogl_rand.h"
#include "vogl_unique_ptr.h"
#include "vogl_file_utils.h"
#include "vogl_find_files.h"

namespace vogl
{
    static random &get_random()
    {
        static random s_random;
        return s_random;
    }

    static uint8_t *read_rand_data(data_stream &rand_stream, uint64_t file_size)
    {
        const uint64_t rand_stream_size = rand_stream.get_size();

        if (file_size > rand_stream_size)
            return NULL;

        uint64_t max_ofs = rand_stream.get_size() - file_size;
        uint64_t src_file_ofs = get_random().urand64_inclusive(0, max_ofs);

        if (!rand_stream.seek(src_file_ofs, false))
            return NULL;

        if (file_size != static_cast<size_t>(file_size))
            return NULL;

        uint8_t *p = static_cast<uint8_t *>(vogl_malloc(static_cast<size_t>(file_size)));
        if (!p)
            return NULL;

        if (rand_stream.read64(p, file_size) != file_size)
        {
            vogl_free(p);
            return NULL;
        }

        return p;
    }

    struct zip_file_desc
    {
        dynamic_string m_name;
        uint64_t m_size;
        uint32_t m_crc;

        zip_file_desc(const dynamic_string &name, uint64_t size, uint32_t crc)
            : m_name(name), m_size(size), m_crc(crc)
        {
        }
    };
    typedef vogl::vector<zip_file_desc> zip_file_desc_vec;

    typedef vogl::vector<uint64_t> uint64_vec;

    static void create_rand_file_sizes(uint64_vec &file_sizes, uint32_t num_files, uint64_t min_file_size = 0, uint64_t max_file_size = cUINT64_MAX)
    {
        file_sizes.resize(num_files);
        for (uint32_t i = 0; i < num_files; i++)
            file_sizes[i] = get_random().urand64_inclusive(min_file_size, max_file_size);
    }

#define FAIL                                      \
    do                                            \
    {                                             \
        printf("Failure on line %u\n", __LINE__); \
        VOGL_ASSERT(0);                         \
        return false;                             \
    } while (0)

    static bool mz_zip_create_random_file_archive(data_stream &rand_stream, const char *pZip_filename, bool zip64, uint64_t min_file_size, uint64_t max_file_size, uint32_t min_files, uint32_t max_files, zip_file_desc_vec &files, mz_zip_archive *pSrc_zip)
    {
        const uint32_t total_files = get_random().urand_inclusive(min_files, max_files);

        uint64_vec file_sizes;
        create_rand_file_sizes(file_sizes, total_files, min_file_size, math::minimum<uint64_t>(rand_stream.get_size(), max_file_size));

        mz_zip_archive zip;
        mz_zip_zero_struct(&zip);

        uint32_t proceeding_size = get_random().urand(0, 1000000);

        printf("Creating archive %s, total files %u\n", pZip_filename, total_files);

        if (!mz_zip_writer_init_file(&zip, pZip_filename, proceeding_size, zip64 ? MZ_ZIP_FLAG_WRITE_ZIP64 : 0))
            FAIL;

        dynamic_string_array filenames;
        for (uint32_t i = 0; i < total_files; i++)
            filenames.push_back(dynamic_string(cVarArg, "xxxxfile_%u", i));

        switch (get_random().irand_inclusive(0, 2))
        {
            case 0:
                break;
            case 1:
                filenames.reverse();
            case 2:
                filenames.shuffle(get_random());
                break;
        }

        dynamic_string temp_filename(cVarArg, "temp_%u.bin", get_random().urand32());

        for (uint32_t i = 0; i < total_files; i++)
        {
            const dynamic_string &name = filenames[i];

            //uint32_t level = get_random().irand_inclusive(0, 10);
            uint32_t level = get_random().irand_inclusive(0, 1);

            // HACK HACK
            //level = 0;

            uint64_t actual_size = file_sizes[i];

            uint8_t *p = read_rand_data(rand_stream, actual_size);
            if (!p)
                FAIL;

            if (actual_size != static_cast<size_t>(actual_size))
            {
                vogl_free(p);
                FAIL;
            }

            if (!zip64)
            {
                if ((zip.m_archive_size + (actual_size * 101ULL) / 100 + mz_zip_get_central_dir_size(&zip) + 4096) > 0xFFFF0000)
                    break;
            }

            bool add_from_mem = get_random().get_bit() != 0;

            printf("Adding file %s, size %s, add from mem: %u\n", name.get_ptr(), uint64_to_string_with_commas(actual_size).get_ptr(), add_from_mem);

            files.push_back(zip_file_desc(name, actual_size, (uint32_t)mz_crc32(MZ_CRC32_INIT, p, static_cast<size_t>(actual_size))));

            dynamic_string comment_str;
            comment_str.reserve(512);
            uint32_t l = get_random().urand_inclusive(0, 300);

            for (uint32_t i2 = 0; i2 < l; i2++)
                comment_str.append_char((char)get_random().urand_inclusive(32, 127));

            if (add_from_mem)
            {
                if (!mz_zip_writer_add_mem_ex(&zip, name.get_ptr(), p, static_cast<size_t>(actual_size), comment_str.get_ptr(), comment_str.get_len(), level, 0, 0))
                {
                    vogl_free(p);
                    FAIL;
                }
            }
            else
            {
                if (!file_utils::write_buf_to_file(temp_filename.get_ptr(), p, static_cast<size_t>(actual_size)))
                {
                    vogl_free(p);
                    FAIL;
                }

                if (!mz_zip_writer_add_file(&zip, name.get_ptr(), temp_filename.get_ptr(), comment_str.get_ptr(), comment_str.get_len(), level))
                {
                    vogl_free(p);
                    FAIL;
                }
            }

            vogl_free(p);
        }

        if (pSrc_zip)
        {
            uint32_t n = get_random().urand_inclusive(0, VOGL_MIN(100, pSrc_zip->m_total_files));

            printf("Copying %u files from source archive\n", n);

            vogl::vector<uint32_t> used_files;

            for (uint32_t i = 0; i < n; i++)
            {
                uint32_t src_file_index;
                for (;;)
                {
                    src_file_index = get_random().urand(0, pSrc_zip->m_total_files);
                    if (used_files.find(src_file_index) < 0)
                        break;
                }
                used_files.push_back(src_file_index);

                mz_zip_archive_file_stat stat;
                if (!mz_zip_file_stat(pSrc_zip, src_file_index, &stat))
                    FAIL;

                if (!zip64)
                {
                    if (zip.m_total_files == 0xFFFF)
                        break;

                    if ((zip.m_archive_size + stat.m_comp_size + mz_zip_get_central_dir_size(&zip) + 4096) > 0xFFFF0000)
                        break;
                }
                else
                {
                    if (zip.m_total_files == 0xFFFFFFFF)
                        break;
                }

                if (!mz_zip_writer_add_from_other_zip(&zip, pSrc_zip, src_file_index))
                    FAIL;
            }
        }

        if (!mz_zip_writer_finalize_archive(&zip))
        {
            mz_zip_writer_end(&zip);
            FAIL;
        }

        printf("Finished archive %s, total files %u, total size %s\n", pZip_filename, total_files, uint64_to_string_with_commas(zip.m_archive_size).get_ptr());

        if (!mz_zip_writer_end(&zip))
            FAIL;

        return true;
    }

#define CHECK(x)                                 \
    if (!(x))                                    \
    {                                            \
        VOGL_ASSERT(0);                        \
        printf("Failed on line %u\n", __LINE__); \
        return false;                            \
    }

    static bool shuffle_order_of_archive(const char *pSrc_zip, const char *pDst_zip)
    {
        mz_zip_archive src_zip;
        mz_zip_zero_struct(&src_zip);

        if (!mz_zip_reader_init_file(&src_zip, pSrc_zip, 0, 0, 0))
            FAIL;

        mz_zip_archive dst_zip;
        mz_zip_zero_struct(&dst_zip);

        if (!mz_zip_writer_init_file(&dst_zip, pDst_zip, 0, MZ_ZIP_FLAG_WRITE_ALLOW_READING | (mz_zip_is_zip64(&src_zip) ? MZ_ZIP_FLAG_WRITE_ZIP64 : 0)))
        {
            mz_zip_reader_end(&src_zip);
            FAIL;
        }

        vogl::vector<uint32_t> file_order(src_zip.m_total_files);
        for (uint32_t i = 0; i < src_zip.m_total_files; i++)
            file_order[i] = i;
        file_order.shuffle(get_random());

        for (uint32_t i = 0; i < src_zip.m_total_files; i++)
        {
            printf("Copying file %u, %3.2f%%\n", file_order[i], (i * 100.0f) / src_zip.m_total_files);
            if (!mz_zip_writer_add_from_other_zip(&dst_zip, &src_zip, file_order[i]))
            {
                mz_zip_reader_end(&src_zip);
                mz_zip_writer_end(&dst_zip);
                FAIL;
            }

            mz_zip_archive_file_stat stat;
            mz_bool status = mz_zip_file_stat(&dst_zip, i, &stat);
            CHECK(status);

            mz_uint32 index = 0;
            status = mz_zip_locate_file(&dst_zip, stat.m_filename, NULL, 0, &index);
            CHECK(status);
            if (index != i)
            {
                dynamic_string_array filenames;
                for (uint32_t j = 0; j < dst_zip.m_total_files; j++)
                {
                    mz_zip_archive_file_stat file_stat;
                    mz_zip_file_stat(&dst_zip, j, &file_stat);
                    filenames.push_back(dynamic_string(file_stat.m_filename));
                }
                filenames.sort();
                uint32_t prev_num = filenames.size();
                filenames.unique();
                uint32_t cur_num = filenames.size();

                CHECK(prev_num != cur_num);
            }

            CHECK(mz_zip_validate_file(&dst_zip, i, 0));
        }

        mz_zip_reader_end(&src_zip);

        if (!mz_zip_writer_finalize_archive(&dst_zip))
        {
            mz_zip_writer_end(&dst_zip);
            FAIL;
        }

        if (!mz_zip_writer_end(&dst_zip))
            FAIL;

        return true;
    }

    bool validate_archives_in_dir(const char *pDir)
    {
        find_files finder;
        if (!finder.find(pDir, find_files::cFlagRecursive | find_files::cFlagAllowFiles))
            return true;

        uint32_t num_failures = 0;
        uint32_t num_good = 0;

        dynamic_string_array failures;

        dynamic_string temp_filename(vogl::file_utils::generate_temp_filename("voglminiz"));

        for (uint32_t file_index = 0; file_index < finder.get_files().size(); file_index++)
        {
            dynamic_string filename(finder.get_files()[file_index].m_fullname);

            printf("Validating archive: %s\n", filename.get_ptr());

            mz_zip_error last_zip_error = MZ_ZIP_NO_ERROR;
            if (!mz_zip_validate_file_archive(filename.get_ptr(), 0, &last_zip_error))
            {
                if ((last_zip_error == MZ_ZIP_UNSUPPORTED_METHOD) || (last_zip_error == MZ_ZIP_UNSUPPORTED_ENCRYPTION))
                {
                    printf("Failed, unsupported method or archive is encrypted\n");
                }
                else
                {
                    printf("FAILED - trying to 7z\n");

                    int result = system(dynamic_string(cVarArg, "7z -p t \"%s\"", filename.get_ptr()).get_ptr());
                    if (result == 0)
                    {
                        printf("7z succeeded but miniz_zip failed, so this is a FAILURE!\n");
                        num_failures++;
                        failures.push_back(filename);
                    }
                    else
                    {
                        printf("7z also failed, so all is ok\n");
                    }
                }
            }
            else
            {
                num_good++;

                printf("Validating with 7z:\n");
                int result = system(dynamic_string(cVarArg, "7z t \"%s\"", filename.get_ptr()).get_ptr());
                if (result)
                    printf("Couldn't validate original archive with 7z!\n");
                else
                {
                    printf("Shuffling archive %s:\n", temp_filename.get_ptr());
                    if (!shuffle_order_of_archive(filename.get_ptr(), temp_filename.get_ptr()))
                        FAIL;

                    printf("Validating with miniz_zip:\n");
                    if (!mz_zip_validate_file_archive(temp_filename.get_ptr(), 0, &last_zip_error))
                        FAIL;

                    printf("Validating with 7z:\n");
                    int result2 = system(dynamic_string(cVarArg, "7z t \"%s\"", temp_filename.get_ptr()).get_ptr());
                    if (result2)
                        FAIL;

                    printf("Ok\n");
                }
            }
        }

        printf("Total failures: %u, total good: %u\n", num_failures, num_good);
        for (uint32_t i = 0; i < failures.size(); i++)
            printf("%s\n", failures[i].get_ptr());
        return true;
    }

    bool mz_zip_test()
    {
        get_random().seed(1000);

        //CHECK(mz_zip_validate_file_archive("/home/richg/temp/blah.zip", 0, NULL));
        //CHECK(mz_zip_validate_file_archive("big.zip", 0, NULL));
        //CHECK(shuffle_order_of_archive("/home/richg/temp/blah.zip", "big.zip"));

        //CHECK(shuffle_order_of_archive("big.zip", "big2.zip"));
        //CHECK(mz_zip_validate_file_archive("big2.zip", 0, NULL));

        //return true;

        CHECK(validate_archives_in_dir("/media/AB80-6D4D/*.zip"));
        return true;

//CHECK(validate_archives_in_dir("/home/richg/dev/raddebugger/bin/temp4/*.zip"));
//return true;

//CHECK(validate_archives_in_dir("/media/Green/dev/*.zip"));

//CHECK(validate_archives_in_dir("/home/richg/temp/*.zip"));

//return true;

//CHECK(mz_zip_validate_file_archive("g1.zip", 0));
//return true;

#if 0
	//void *p = calloc(0xFFFFFFFF, 1);
	//bool status = mz_zip_add_mem_to_archive_file_in_place("x2.zip", "1.bin", p, 0xFFFFFFFF, NULL, 0, 1, false);
	//CHECK(status);

	CHECK(mz_zip_validate_file_archive("x2.zip", MZ_ZIP_FLAG_VALIDATE_LOCATE_FILE_FLAG));

	CHECK(mz_zip_validate_file_archive("x3.zip", MZ_ZIP_FLAG_VALIDATE_LOCATE_FILE_FLAG));

	CHECK(mz_zip_validate_file_archive("x.zip", MZ_ZIP_FLAG_VALIDATE_LOCATE_FILE_FLAG));

	mz_zip_archive zip_reader;
	mz_zip_zero_struct(&zip_reader);
	CHECK(mz_zip_reader_init_file(&zip_reader, "x2.zip", 0));

	mz_zip_archive zip;
	mz_zip_zero_struct(&zip);

	CHECK(mz_zip_writer_init_file(&zip, "x3.zip", 0, MZ_ZIP_FLAG_WRITE_ZIP64));
	CHECK(mz_zip_writer_add_from_zip_reader(&zip, &zip_reader, 0));
	CHECK(mz_zip_writer_finalize_archive(&zip));
	CHECK(mz_zip_writer_end(&zip));

	exit(0);

	//bool success = validate_archive("z2.zip", true);
	//CHECK(success);
	//exit(0);
#endif

        random &rm = get_random();

        mz_zip_archive src_zip64;
        mz_zip_zero_struct(&src_zip64);
        CHECK(mz_zip_reader_init_file(&src_zip64, "/home/richg/temp/a.zip", 0, 0, 0));
        //CHECK(mz_zip_is_zip64(&src_zip64));

        mz_zip_archive src_zip32;
        mz_zip_zero_struct(&src_zip32);
        CHECK(mz_zip_reader_init_file(&src_zip32, "/home/richg/temp/a.zip", 0, 0, 0));
        //CHECK(!mz_zip_is_zip64(&src_zip32));

        cfile_stream rand_stream("/home/richg/temp/output1.bin", cDataStreamReadable | cDataStreamSeekable);
        if (!rand_stream.is_opened())
            return false;

        int zip_base = rm.urand_inclusive('a', 'z');
        int start_seed = rm.urand(1, 10000);

        dynamic_string temp_filename(cVarArg, "temp_%u.bin", rm.urand32());

        for (uint32_t t = 0; t < 10000; t++)
        {
            uint32_t seed = t + start_seed;
            printf("******* Pass %u, seed %u\n", t, seed);

            rm.seed(seed);

            dynamic_string zip_filename(cVarArg, "%c%u.zip", zip_base, t + 1);

            dynamic_string full_zip_filename(zip_filename);
            file_utils::full_path(full_zip_filename);

            bool zip64 = rm.get_bit() != 0;

            bool test_big_files = rm.get_bit() != 0;

            uint32_t max_files;
            if (test_big_files)
                max_files = (uint32_t)rm.urand64_inclusive(0, 10);
            else
                max_files = (uint32_t)rm.urand64_inclusive(0, zip64 ? 200000 : 65535);

            printf("Max files: %u, zip64: %u\n", max_files, zip64);

            bool success;

            uint64_t min_file_size = 0;
            uint64_t max_file_size = 1000000; //zip64 ? 0xFFFFFFFFFF : 0xFFFFFFFF;
            if (!test_big_files)
            {
                max_file_size = 1024;
            }

            zip_file_desc_vec file_descs;
            success = mz_zip_create_random_file_archive(rand_stream, zip_filename.get_ptr(), zip64, min_file_size, max_file_size, 0, max_files, file_descs, zip64 ? (rm.get_bit() ? &src_zip64 : &src_zip32) : &src_zip32);
            CHECK(success);

            uint32_t num_actual_files = file_descs.size();

            printf("******* Validating archive %s using miniz_zip\n", full_zip_filename.get_ptr());

            mz_zip_error last_zip_error = MZ_ZIP_NO_ERROR;
            success = (mz_zip_validate_file_archive(zip_filename.get_ptr(), ((rm.get_bit() && (file_descs.size() < 30000)) ? MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY : 0) | MZ_ZIP_FLAG_VALIDATE_LOCATE_FILE_FLAG, &last_zip_error)) != 0;
            CHECK(success);

            printf("******* Validating archive %s using unzip\n", full_zip_filename.get_ptr());

            int result = system(dynamic_string(cVarArg, "unzip -t \"%s\"", full_zip_filename.get_ptr()).get_ptr());
            if ((num_actual_files == 0) && (result == 256))
            {
            }
            else
            {
                CHECK(result == 0);
            }

            printf("******* Validating archive %s using 7z\n", full_zip_filename.get_ptr());

            result = system(dynamic_string(cVarArg, "7z t \"%s\"", full_zip_filename.get_ptr()).get_ptr());
            CHECK(result == 0);

            printf("Shuffling archive %s:\n", full_zip_filename.get_ptr());
            if (!shuffle_order_of_archive(full_zip_filename.get_ptr(), temp_filename.get_ptr()))
                FAIL;

            printf("Validating with miniz_zip:\n");
            if (!mz_zip_validate_file_archive(temp_filename.get_ptr(), 0, &last_zip_error))
                FAIL;

            printf("Validating with 7z:\n");
            result = system(dynamic_string(cVarArg, "7z t \"%s\"", temp_filename.get_ptr()).get_ptr());
            if (result)
                FAIL;
        }

        // this func potentially leaks if something fails
        mz_zip_reader_end(&src_zip32);
        mz_zip_reader_end(&src_zip64);

        return true;
    }

} // namespace vogl
