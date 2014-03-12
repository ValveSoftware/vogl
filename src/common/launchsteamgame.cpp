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
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include <vogl_core.h>
#include <vogl_json.h>

#include <iomanip>
#include <sstream>

#include "commands.h"
#include "launchsteamgame.h"


std::string url_encode(const std::string &value);

int 
LaunchSteamGame(unsigned int buffer_size, char *buffer)
{
    vogl::json_document cur_doc;
    const char *pGameId;
    const char *pvogl_cmd_params = NULL;
    const char *gameport = NULL;
    int bitness = 0;

    if (0 >= buffer_size)
        return -1;

    cur_doc.deserialize(buffer);

    vogl::json_node *pjson_node = cur_doc.get_root()->find_child_object("parameters");

    pGameId = pjson_node->value_as_string_ptr("gameid", "");
    pvogl_cmd_params = pjson_node->value_as_string_ptr("vogl_cmd_params", NULL);
    bitness = pjson_node->value_as_int("bitness", 0);
    gameport = pjson_node->value_as_string_ptr("gameport", NULL);

    std::string VOGL_CMD_LINE;

    // Get the full path to our executable.
    // set up LD_PRELOAD string
    std::string LD_PRELOAD = "LD_PRELOAD=";
    if (32 == bitness)
        LD_PRELOAD += "libvogltrace32.so:$LD_PRELOAD";
    else
        LD_PRELOAD += "libvogltrace64.so:$LD_PRELOAD";

    printf("\n%s\n", LD_PRELOAD.c_str());

    // set up VOGL_CMD_LINE string
    if ( (NULL != pvogl_cmd_params && pvogl_cmd_params[0] != '\0') 
      || (gameport != NULL && gameport[0] != '\0'))
    {
        VOGL_CMD_LINE += "VOGL_CMD_LINE=\"";

        if (NULL != pvogl_cmd_params && pvogl_cmd_params[0] != '\0')
        {
            VOGL_CMD_LINE += pvogl_cmd_params;
        }

        if (NULL != gameport && gameport[0] != '\0')
        {
            VOGL_CMD_LINE += " --vogl_traceport ";
            VOGL_CMD_LINE += gameport;
        }

        VOGL_CMD_LINE += "\"";

        printf("\n%s\n", VOGL_CMD_LINE.c_str());
    }

    std::string steam_str = "steam steam://run/";
    steam_str += pGameId;
    steam_str += "//";
    std::string steam_args = LD_PRELOAD + " " + VOGL_CMD_LINE + " %command%";
    std::string steam_fullcmd = steam_str + url_encode(steam_args);

    printf("\nlaunch string:\n%s\n", steam_fullcmd.c_str());

    system(steam_fullcmd.c_str());

    return (NULL == gameport? 0: atoi(gameport));
}



std::string
url_encode(const std::string &value)
{
    std::ostringstream escaped;

    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i)
    {
        std::string::value_type c = (*i);

        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            escaped << c;
        else if (c == ' ')
            escaped << "%20";
        else
            escaped << '%' << std::setw(2) << ((int) c) << std::setw(0);
    }

    return escaped.str();
}



int
LaunchSteamGameReq(const char *gameid, const char *vogl_cmd_params, int bitness, const char *gameport, unsigned int *pbuffer_size, char **pbuffer)
{
    vogl::json_document cur_doc;
    vogl::dynamic_string dst;

    char *pbBuff;
    unsigned int cbBuff;

    vogl::json_node &meta_node = cur_doc.get_root()->add_object("parameters");
    meta_node.add_key_value("gameid", gameid);
    meta_node.add_key_value("vogl_cmd_params", vogl_cmd_params);
    meta_node.add_key_value("bitness", bitness);
    meta_node.add_key_value("gameport", gameport);

    cur_doc.serialize(dst);

    cbBuff = dst.get_len() + 1 + sizeof(int32_t);
    pbBuff = (char *)malloc(cbBuff);
    if (NULL == pbBuff)
    {
        printf("OOM\n");
        return -1;
    }

    //  First part of buffer is the command id
    *((int32_t *)pbBuff) = LAUNCHSTEAMGAME;
    strncpy((char *)(pbBuff+sizeof(int32_t)), dst.get_ptr(), cbBuff - sizeof(int32_t));

    *pbuffer = pbBuff;
    *pbuffer_size = cbBuff;

    return 0;
}

