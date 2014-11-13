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
#include "vogl_port.h"
#include "vogl_uuid.h"

#if defined(VOGL_USE_WIN32_API)
	#include <rpc.h>
    #pragma comment(lib, "Rpcrt4.lib")
#endif

namespace vogl
{
    md5_hash gen_uuid()
    {
    #if defined(PLATFORM_LINUX) || defined(PLATFORM_OSX)

        uint32_t buf[] = { 0xABCDEF, 0x12345678, 0xFFFECABC, 0xABCDDEF0 };
        plat_rand_s(buf, VOGL_ARRAY_SIZE(buf));

        return md5_hash(buf[0], buf[1], buf[2], buf[3]);

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
