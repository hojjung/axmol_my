/****************************************************************************
 Copyright (c) 2019-present Axmol Engine contributors (see AUTHORS.md).

 https://axmol.dev/

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#pragma once

#include "axmol/3d/Bundle3DData.h"
#include "axmol/platform/PlatformMacros.h"

#include <string_view>

namespace ax
{

/** Runtime glTF 2.0 loader. Host-side conversion and encoding are deliberately
 * kept out of this API so WebAssembly builds only contain parsing and decode.
 */
class AX_DLL GltfLoader final
{
public:
    static bool isGltfPath(std::string_view path);

    static bool load(std::string_view path, NodeDatas& nodeDatas, MeshDatas& meshDatas, MaterialDatas& materialDatas);

    static bool loadAnimation(std::string_view path, std::string_view animationName, Animation3DData& animationData);

    GltfLoader() = delete;
};

}  // namespace ax
