// File: ktxtool.cpp

#include <stdlib.h>
#include <stdio.h>

#include "vogl_core.h"
#include "vogl_colorized_console.h"
#include "vogl_command_line_params.h"
#include "vogl_cfile_stream.h"
#include "vogl_ktx_texture.h"
#include "vogl_miniz.h"
#include "vogl_file_utils.h"
#include "vogl_image.h"
#include "vogl_image_utils.h"

#include "pxfmt_gl.h"
#include "pxfmt.h"

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
    vogl_printf("ktxtool ");
    if (sizeof(void *) > 4)
        vogl_printf("64-bit ");
    else
        vogl_printf("32-bit ");
#ifdef VOGL_BUILD_DEBUG
    vogl_printf("Debug ");
#else
    vogl_printf("Release ");
#endif
    vogl_printf("Built %s %s\n", __DATE__, __TIME__);
}

//----------------------------------------------------------------------------------------------------------------------
// tool_print_help
//----------------------------------------------------------------------------------------------------------------------
static void tool_print_help()
{
    vogl_printf("Usage: ktxtool [ -option ... ] input_file.ktx output_prefix [ -option ... ]\n");
    vogl_printf("Command line options may begin with single minus \"-\" or double minus \"--\"\n");

    vogl_printf("\nCommand line options:\n");

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
        vogl_error_printf("Failed parsing command line parameters!\n");
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
        vogl_error_printf("2 parameters required\n");
        return EXIT_FAILURE;
    }

    dynamic_string in_file(g_command_line_params().get_value_as_string_or_empty("", 1));
    dynamic_string out_prefix(g_command_line_params().get_value_as_string_or_empty("", 2));

    printf("Input file: %s\nOutput prefix: %s\n", in_file.get_ptr(), out_prefix.get_ptr());

    cfile_stream in_stream;
    if (!in_stream.open(in_file.get_ptr()))
    {
        vogl_error_printf("Failed opening input file \"%s\"!\n", in_file.get_ptr());
        return EXIT_FAILURE;
    }

    ktx_texture tex;

    data_stream_serializer in_stream_serializer(in_stream);
    if (!tex.read_from_stream(in_stream_serializer))
    {
        vogl_error_printf("Failed parsing input KTX file \"%s\"!\n", in_file.get_ptr());
        return EXIT_FAILURE;
    }

    vogl_printf("KTX header:%s Format=0x%04x, Type=0x%04x,\n\t    "
                "width=%d, height=%d, depth=%d, # mips=%d, # faces=%d, ArraySize=%d\n",
                tex.get_opposite_endianness() ? " OppositeEndianness," : "",
                tex.get_ogl_fmt(), tex.get_ogl_type(),
                tex.get_width(), tex.get_height(), tex.get_depth(),
                tex.get_num_mips(), tex.get_num_faces(), tex.get_array_size());

    pxfmt_sized_format src_pxfmt;
    if (tex.is_compressed())
    {
        vogl_printf("\t    InternalFormat=0x%04x, BaseInternalFormat=0x%04x,\n\t    "
                    "Compressed texture: BlockDimension=%d x %d, BytesPerBlock=%d\n",
                    tex.get_ogl_internal_fmt(), tex.get_ogl_base_fmt(),
                    tex.get_block_dim_x(), tex.get_block_dim_y(), tex.get_bytes_per_block());

        src_pxfmt = validate_internal_format(tex.get_ogl_internal_fmt());
        if (src_pxfmt == PXFMT_INVALID)
        {
            vogl_error_printf("Unsupported KTX compressed format: 0x%X\n",
                              tex.get_ogl_internal_fmt());
            return EXIT_FAILURE;
        }
    }
    else
    {
        src_pxfmt = validate_format_type_combo(tex.get_ogl_fmt(),
                                               tex.get_ogl_type());
        if (src_pxfmt == PXFMT_INVALID)
        {
            vogl_error_printf("Unsupported KTX format/type: 0x%X 0x%X\n",
                              tex.get_ogl_fmt(), tex.get_ogl_type());
            return EXIT_FAILURE;
        }
    }

    // TODO: This is a total work in progress!
    for (uint32_t array_index = 0; array_index < tex.get_array_size(); array_index++)
    {
        for (uint32_t face_index = 0; face_index < tex.get_num_faces(); face_index++)
        {
            for (uint32_t mip_level = 0; mip_level < tex.get_num_mips(); mip_level++)
            {
                uint32_t mip_depth = math::maximum<uint32_t>(1U, tex.get_depth() >> mip_level);

                for (uint32_t zslice_index = 0; zslice_index < mip_depth; zslice_index++)
                {
                    uint32_t mip_width = math::maximum<uint32_t>(1U, tex.get_width() >> mip_level);
                    uint32_t mip_height = math::maximum<uint32_t>(1U, tex.get_height() >> mip_level);

                    uint32_t image_index = tex.get_image_index(mip_level, array_index, face_index, zslice_index);
                    if (image_index >= tex.get_num_images())
                        continue;

                    const uint8_vec &level_data = tex.get_image_data(image_index);
                    if (level_data.is_empty())
                        continue;

                    // Get information about the source:
                    bool has_red;
                    bool has_green;
                    bool has_blue;
                    bool has_alpha;
                    bool has_depth;
                    bool has_stencil;
                    bool has_large_components;
                    bool is_floating_point;
                    bool is_integer;
                    bool is_compressed;
                    unsigned int bytes_per_pixel;
                    unsigned int bytes_per_compressed_block;
                    unsigned int block_size;
                    query_pxfmt_sized_format(src_pxfmt, &has_red, &has_green, &has_blue, &has_alpha,
                                             &has_depth, &has_stencil, &has_large_components, &is_floating_point,
                                             &is_integer, &is_compressed, &bytes_per_pixel,
                                             &bytes_per_compressed_block, &block_size);
                    size_t src_size = bytes_per_pixel * mip_width * mip_height;

                    uint32_t temp_size = mip_width * sizeof(uint32_t) * mip_height;
                    uint8_vec temp_buf(temp_size);
                    temp_buf.push_back(0xAB);
                    temp_buf.push_back(0xCD);

                    pxfmt_sized_format temp_pxfmt = PXFMT_RGBA8_UNORM;

                    pxfmt_conversion_status status;
                    if (tex.is_compressed())
                    {
                        // FIXME/TBD/TODO: CONSIDER MERGING THIS FUNCTION BACK
                        // INTO THE NEXT FUNCTION (i.e. have one function for
                        // both compressed and non-compressed src data):
                        status = pxfmt_decompress_pixels(temp_buf.get_ptr(), level_data.get_ptr(),
                                                         mip_width, mip_height,
                                                         temp_pxfmt, src_pxfmt, temp_size, src_size);
                    }
                    else
                    {
                        status = pxfmt_convert_pixels(temp_buf.get_ptr(), level_data.get_ptr(),
                                                      mip_width, mip_height,
                                                      temp_pxfmt, src_pxfmt, temp_size, src_size);
                    }

                    if (status != PXFMT_CONVERSION_SUCCESS)
                    {
                        vogl_error_printf("pxfmt_convert_pixels() returned a non-success status of %d!\n", status);
                        return EXIT_FAILURE;
                    }

                    if ((temp_buf[temp_buf.size() - 2] != 0xAB) || (temp_buf[temp_buf.size() - 1] != 0xCD))
                    {
                        vogl_error_printf("pxfmt_convert_pixels() overwrote its output buffer!\n");
                        return EXIT_FAILURE;
                    }

                    image_u8 rgb_img(mip_width, mip_height);
                    image_u8 alpha_img(mip_width, mip_height);

                    for (uint32_t y = 0; y < mip_height; y++)
                    {
                        const color_quad_u8 *pPixel = reinterpret_cast<color_quad_u8 *>(temp_buf.get_ptr() + y * mip_width * sizeof(uint32_t));

                        for (uint32_t x = 0; x < mip_width; x++)
                        {
                            rgb_img.set_pixel_unclipped(x, y, *pPixel);
                            alpha_img.set_pixel_unclipped(x, y, color_quad_u8((*pPixel)[3]));

                            ++pPixel;
                        }
                    }

                    dynamic_string rgb_output_filename(cVarArg, "%s_array_%u_face_%u_mip_%u_zslice_%u_rgb.png",
                        out_prefix.get_ptr(), array_index, face_index, mip_level, zslice_index);

                    vogl_message_printf("Writing RGB output file %s\n", rgb_output_filename.get_ptr());

                    if (!image_utils::write_to_file(rgb_output_filename.get_ptr(), rgb_img, image_utils::cWriteFlagIgnoreAlpha))
                    {
                        vogl_error_printf("Failed writing to output file %s\n", rgb_output_filename.get_ptr());
                        return EXIT_FAILURE;
                    }

                    // TODO: We always write an alpha image, although there may not be any alpha data in the source image's pixel format.
                    dynamic_string alpha_output_filename(cVarArg, "%s_array_%u_face_%u_mip_%u_zslice_%u_alpha.png",
                        out_prefix.get_ptr(), array_index, face_index, mip_level, zslice_index);

                    vogl_message_printf("Writing alpha output file %s\n", alpha_output_filename.get_ptr());

                    if (!image_utils::write_to_file(alpha_output_filename.get_ptr(), alpha_img, image_utils::cWriteFlagIgnoreAlpha | image_utils::cWriteFlagGrayscale, 0))
                    {
                        vogl_error_printf("Failed writing to output file %s\n", rgb_output_filename.get_ptr());
                        return EXIT_FAILURE;
                    }

                } // zslice_index

            } // mip_level

        } // face_index

    } // array_index


    return EXIT_SUCCESS;
}





































