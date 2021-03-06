/**
 * Copyright (c) 2018 Melown Technologies SE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * *  Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef mapproxy_tiling_tiling_hpp_included_
#define mapproxy_tiling_tiling_hpp_included_

#include <boost/filesystem.hpp>

#include "vts-libs/registry.hpp"
#include "vts-libs/vts/tileindex.hpp"
#include "vts-libs/vts/basetypes.hpp"

namespace tiling {

struct Config {
    int tileSampling;
    bool parallel;
    bool forceWatertight;

    Config()
        : tileSampling(128), parallel(true), forceWatertight(false)
    {}
};

vtslibs::vts::TileIndex
generate(const boost::filesystem::path &dataset
         , const vtslibs::registry::ReferenceFrame &referenceFrame
         , const vtslibs::vts::LodRange &lodRange
         , const vtslibs::vts::LodTileRange::list &tileRanges
         , const Config &config);

} // namespace tiling

#endif // mapproxy_tiling_tiling_hpp_included_
