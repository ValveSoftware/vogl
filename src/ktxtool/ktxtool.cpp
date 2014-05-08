// File: ktxtool.cpp

#include <stdlib.h>
#include <stdio.h>

#include "pxfmt.h"

#include "vogl_core.h"
#include "vogl_colorized_console.h"
#include "vogl_command_line_params.h"
#include "vogl_cfile_stream.h"
#include "vogl_ktx_texture.h"
#include "vogl_miniz.h"
#include "vogl_file_utils.h"
#include "vogl_image.h"
#include "vogl_image_utils.h"

using namespace vogl;

//----------------------------------------------------------------------------------------------------------------------
// command line params
//----------------------------------------------------------------------------------------------------------------------
static command_line_param_desc g_command_line_param_descs[] =
    {
        { "help", 0, false, "Display this help" },
        { "?", 0, false, "Display this help" },
    };

//----------------------------------------------------------------------------------------------------------------------
// tool_print_title
//----------------------------------------------------------------------------------------------------------------------
static void tool_print_title()
{
    console::printf("ktxtool ");
    if (sizeof(void *) > 4)
        console::printf("64-bit ");
    else
        console::printf("32-bit ");
#ifdef VOGL_BUILD_DEBUG
    console::printf("Debug ");
#else
    console::printf("Release ");
#endif
    console::printf("Built %s %s\n", __DATE__, __TIME__);
}

//----------------------------------------------------------------------------------------------------------------------
// tool_print_help
//----------------------------------------------------------------------------------------------------------------------
static void tool_print_help()
{
    console::printf("Usage: ktxtool [ -option ... ] input_file.ktx output_prefix [ -option ... ]\n");
    console::printf("Command line options may begin with single minus \"-\" or double minus \"--\"\n");

    console::printf("\nCommand line options:\n");

    dump_command_line_info(VOGL_ARRAY_SIZE(g_command_line_param_descs), g_command_line_param_descs, "--");
}

//----------------------------------------------------------------------------------------------------------------------
// init_command_line_params
//----------------------------------------------------------------------------------------------------------------------
static bool init_command_line_params(int argc, char *argv[])
{
    command_line_params::parse_config parse_cfg;
    parse_cfg.m_single_minus_params = true;
    parse_cfg.m_double_minus_params = true;

    if (!g_command_line_params().parse(get_command_line_params(argc, argv), VOGL_ARRAY_SIZE(g_command_line_param_descs), g_command_line_param_descs, parse_cfg))
    {
        console::error("%s: Failed parsing command line parameters!\n", VOGL_FUNCTION_INFO_CSTR);
        return false;
    }

    if (g_command_line_params().get_value_as_bool("help") || g_command_line_params().get_value_as_bool("?"))
    {
        tool_print_help();
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// main
//----------------------------------------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
    // Initialize vogl_core.
    vogl_core_init();

    colorized_console::init();
    colorized_console::set_exception_callback();

    tool_print_title();

    if (!init_command_line_params(argc, argv))
        return EXIT_FAILURE;

    if (g_command_line_params().get_count("") != 3)
    {
        console::error("2 parameters required\n");
        return EXIT_FAILURE;
    }

    dynamic_string in_file(g_command_line_params().get_value_as_string_or_empty("", 1));
    dynamic_string out_prefix(g_command_line_params().get_value_as_string_or_empty("", 2));

    printf("Input file: %s\nOutput prefix: %s\n", in_file.get_ptr(), out_prefix.get_ptr());

    cfile_stream in_stream;
    if (!in_stream.open(in_file.get_ptr()))
    {
        console::error("Failed opening input file \"%s\"!\n", in_file.get_ptr());
        return EXIT_FAILURE;
    }

    ktx_texture tex;

    data_stream_serializer in_stream_serializer(in_stream);
    if (!tex.read_from_stream(in_stream_serializer))
    {
        console::error("Failed parsing input KTX file \"%s\"!\n", in_file.get_ptr());
        return EXIT_FAILURE;
    }

    if (tex.is_compressed())
    {
        //get_ogl_internal_fmt()
        console::error("Compressed KTX files are not supported because the pxfmt lib doesn't support compressed formats yet.\n");
        return EXIT_FAILURE;
    }

    pxfmt_sized_format src_pxfmt = validate_format_type_combo(tex.get_ogl_fmt(), tex.get_ogl_type());
    if (src_pxfmt == PXFMT_INVALID)
    {
        console::error("Unsupported KTX format/type: 0x%X 0x%X\n", tex.get_ogl_fmt(), tex.get_ogl_type());
        return EXIT_FAILURE;
    }

    // TODO: This is a total work in progress!
    for (uint array_index = 0; array_index < tex.get_array_size(); array_index++)
    {
        for (uint face_index = 0; face_index < tex.get_num_faces(); face_index++)
        {
            for (uint mip_level = 0; mip_level < tex.get_num_mips(); mip_level++)
            {
                uint mip_depth = math::maximum<uint>(1U, tex.get_depth() >> mip_level);

                for (uint zslice_index = 0; zslice_index < mip_depth; zslice_index++)
                {
                    uint mip_width = math::maximum<uint>(1U, tex.get_width() >> mip_level);
                    uint mip_height = math::maximum<uint>(1U, tex.get_height() >> mip_level);

                    uint image_index = tex.get_image_index(mip_level, array_index, face_index, zslice_index);
                    if (image_index >= tex.get_num_images())
                        continue;

                    const uint8_vec &level_data = tex.get_image_data(image_index);
                    if (level_data.is_empty())
                        continue;

                    uint temp_size = mip_width * sizeof(uint32) * mip_height;

                    uint8_vec temp_buf(temp_size);
                    temp_buf.push_back(0xAB);
                    temp_buf.push_back(0xCD);

                    pxfmt_sized_format temp_pxfmt = PXFMT_RGBA8_UNORM;
                    pxfmt_convert_pixels(temp_buf.get_ptr(), level_data.get_ptr(), mip_width, mip_height, src_pxfmt, temp_pxfmt);

                    if ((temp_buf[temp_buf.size() - 2] != 0xAB) || (temp_buf[temp_buf.size() - 1] != 0xCD))
                    {
                        console::error("pxfmt_convert_pixels() overwrote its output buffer!\n");
                        return EXIT_FAILURE;
                    }

                    image_u8 rgb_img(mip_width, mip_height);
                    image_u8 alpha_img(mip_width, mip_height);

                    for (uint y = 0; y < mip_height; y++)
                    {
                        const color_quad_u8 *pPixel = reinterpret_cast<color_quad_u8 *>(temp_buf.get_ptr() + y * mip_width * sizeof(uint32));

                        for (uint x = 0; x < mip_width; x++)
                        {
                            rgb_img.set_pixel_unclipped(x, y, *pPixel);
                            alpha_img.set_pixel_unclipped(x, y, color_quad_u8((*pPixel)[3]));

                            ++pPixel;
                        }
                    }

                    dynamic_string rgb_output_filename(cVarArg, "%s_array_%u_face_%u_mip_%u_zslice_%u_rgb.png",
                        out_prefix.get_ptr(), array_index, face_index, mip_level, zslice_index);

                    console::info("Writing RGB output file %s\n", rgb_output_filename.get_ptr());

                    if (!image_utils::write_to_file(rgb_output_filename.get_ptr(), rgb_img, image_utils::cWriteFlagIgnoreAlpha))
                    {
                        console::error("Failed writing to output file %s\n", rgb_output_filename.get_ptr());
                        return EXIT_FAILURE;
                    }

                    // TODO: We always write an alpha image, although there may not be any alpha data in the source image's pixel format.
                    dynamic_string alpha_output_filename(cVarArg, "%s_array_%u_face_%u_mip_%u_zslice_%u_alpha.png",
                        out_prefix.get_ptr(), array_index, face_index, mip_level, zslice_index);

                    console::info("Writing alpha output file %s\n", alpha_output_filename.get_ptr());

                    if (!image_utils::write_to_file(alpha_output_filename.get_ptr(), alpha_img, image_utils::cWriteFlagIgnoreAlpha | image_utils::cWriteFlagGrayscale, 0))
                    {
                        console::error("Failed writing to output file %s\n", rgb_output_filename.get_ptr());
                        return EXIT_FAILURE;
                    }

                } // zslice_index

            } // mip_level

        } // face_index

    } // array_index


    return EXIT_SUCCESS;
}





































