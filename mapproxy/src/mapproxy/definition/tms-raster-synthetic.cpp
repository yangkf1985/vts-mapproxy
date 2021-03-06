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

#include <boost/lexical_cast.hpp>
#include <boost/utility/in_place_factory.hpp>

#include "utility/premain.hpp"

#include "jsoncpp/json.hpp"
#include "jsoncpp/as.hpp"

#include "tms.hpp"

namespace resource {

namespace {

void parseDefinition(TmsRasterSynthetic &def, const Json::Value &value)
{
    std::string s;

    if (value.isMember("mask")) {
        def.mask = boost::in_place();
        Json::get(*def.mask, value, "mask");
    }

    if (value.isMember("format")) {
        Json::get(s, value, "format");
        try {
            def.format = boost::lexical_cast<RasterFormat>(s);
        } catch (const boost::bad_lexical_cast&) {
            utility::raise<Json::Error>
                ("Value stored in format is not RasterFormat value");
        }
    }
}

void buildDefinition(Json::Value &value, const TmsRasterSynthetic &def)
{
    if (def.mask) {
        value["mask"] = *def.mask;
    }
    value["format"] = boost::lexical_cast<std::string>(def.format);
}

} // namespace

void TmsRasterSynthetic::parse(const Json::Value &value)
{
    TmsCommon::parse(value);
    parseDefinition(*this, value);
}

void TmsRasterSynthetic::build(Json::Value &value) const
{
    TmsCommon::build(value);
    buildDefinition(value, *this);
}

Changed TmsRasterSynthetic::changed_impl(const DefinitionBase &o) const
{
    const auto &other(o.as<TmsRasterSynthetic>());

    // non-safe changes first
    if (mask != other.mask) { return Changed::yes; }

    // format can change
    if (format != other.format) { return Changed::safely; }

    return TmsCommon::changed_impl(other);
}

} // namespace resource
