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
#include "toclientmsg.h"


int
PrintToClientMsg(unsigned int buffer_size, char *buffer)
{
    vogl::json_document cur_doc;
    int urgency = 0;
    const char *origin_app;
    const char *message;

    if (0 == buffer_size)
        return -1;

    cur_doc.deserialize(buffer);

    vogl::json_node *pjson_node = cur_doc.get_root()->find_child_object("parameters");

    urgency = pjson_node->value_as_int("urgency", 0);
    origin_app = pjson_node->value_as_string_ptr("origin", "unknown");
    message = pjson_node->value_as_string_ptr("message", "No Message");

    if ( 0 == urgency )
        printf("%s: %s\n", origin_app, message);
    else
        fprintf(stderr, "%s: %s\n", origin_app, message);

    return 0;
}

int ToClientMsgReq(char *origin_app, int urgency, const char *message, unsigned int *pcbBuff, char **ppbBuff)
{
    vogl::json_document cur_doc;
    vogl::dynamic_string dst;

    char *pbBuff;
    int cbBuff;

    vogl::json_node &meta_node = cur_doc.get_root()->add_object("parameters");

    meta_node.add_key_value("urgency", urgency);
    meta_node.add_key_value("origin", origin_app);
    meta_node.add_key_value("message", message);

    cur_doc.serialize(dst);

    cbBuff = dst.get_len() + 1 + sizeof(int32_t);
    pbBuff = (char *)malloc(cbBuff);
    if (NULL == pbBuff)
    {
        return -1;
    }

    //  First part of buffer is the command id
    *((int32_t *)pbBuff) = TOCLIENTMSG;
    strncpy((char *)(pbBuff+sizeof(int32_t)), dst.get_ptr(), cbBuff - sizeof(int32_t));

    *ppbBuff = pbBuff;
    *pcbBuff = cbBuff;

    return 0;  
}

