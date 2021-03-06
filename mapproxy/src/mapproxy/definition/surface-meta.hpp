/**
 * Copyright (c) 2019 Melown Technologies SE
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

#ifndef mapproxy_definition_surface_meta_hpp_included_
#define mapproxy_definition_surface_meta_hpp_included_

#include "../resource.hpp"

// fwd
namespace Json { class Value; }

namespace resource {

// meta surface: combines existing surface with existing TMS

class SurfaceMeta : public DefinitionBase {
public:
    Resource::Id surface;
    Resource::Id tms;

    static constexpr Resource::Generator::Type type
        = Resource::Generator::Type::surface;

    static constexpr char driverName[] = "surface-meta";

protected:
    virtual void from_impl(const Json::Value &value);
    virtual void to_impl(Json::Value &value) const;
    virtual Changed changed_impl(const DefinitionBase &other) const;
    virtual Resource::Id::list needsResources_impl() const;
    virtual bool needsRanges_impl() const { return false; }
};

} // namespace resource

#endif // mapproxy_definition_surface_meta_hpp_included_

