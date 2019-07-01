/**
 * Copyright (c) 2017 Melown Technologies SE
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

#include <algorithm>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "utility/premain.hpp"
#include "utility/raise.hpp"
#include "utility/format.hpp"
#include "utility/path.hpp"

#include "math/transform.hpp"

#include "geo/featurelayers.hpp"
#include "geometry/meshop.hpp"

#include "jsoncpp/as.hpp"
#include "jsoncpp/io.hpp"

#include "semantic/io.hpp"
#include "semantic/mesh.hpp"

#include "vts-libs/storage/fstreams.hpp"
#include "vts-libs/registry/json.hpp"

#include "../support/tileindex.hpp"
#include "../support/revision.hpp"
#include "../support/geo.hpp"

#include "geodata-semantic-tiled.hpp"
#include "factory.hpp"
#include "metatile.hpp"
#include "files.hpp"
#include "../gdalsupport/custom.hpp"

namespace ba = boost::algorithm;
namespace fs = boost::filesystem;
namespace vr = vtslibs::registry;

namespace generator {

namespace {

struct Factory : Generator::Factory {
    virtual Generator::pointer create(const Generator::Params &params)
    {
        return std::make_shared<GeodataSemanticTiled>(params);
    }

private:
    static utility::PreMain register_;
};

utility::PreMain Factory::register_([]()
{
    Generator::registerType<GeodataSemanticTiled>(std::make_shared<Factory>());
});

} // namespace

GeodataSemanticTiled::GeodataSemanticTiled(const Params &params)
    : Generator(params)
    , definition_(this->resource().definition<Definition>())
    , dem_(absoluteDataset(definition_.dem.dataset + "/dem")
           , definition_.dem.geoidGrid)
    , styleUrl_(definition_.styleUrl)
    , dataset_(definition_.dataset)
{
    auto ds(geo::GeoDataset::open(dem_.dataset));

    if (styleUrl_.empty()) {
        styleUrl_ = "style.json";
    } else if (ba::istarts_with(styleUrl_, "file:")) {
        // pseudo file URL
        stylePath_ = absoluteDataset(styleUrl_.substr(5));
        styleUrl_ = "style.json";
    }

    // load geodata only if there is no enforced change
    if (changeEnforced()) {
        LOG(info1) << "Generator for <" << id() << "> not ready.";
        return;
    }

    LOG(info1) << "Generator for <" << id() << "> not ready.";
}

namespace {

class LayerBuilder {
public:
    LayerBuilder(const semantic::World &world)
        : world_(world), fid_()
        , materials_(semantic::materials())
    {
        semantic::mesh(world, semantic::MeshConfig()
                       , [this](auto&&... args) {
                           this->mesh(std::forward<decltype(args)>(args)...);
                       }
                       , 2);
    }

    geo::FeatureLayers featureLayers() {
        geo::FeatureLayers fl;
        for (auto &item : layers_) {
            fl.layers.emplace_back(std::move(item.second));
        }
        return fl;
    }

private:
    using Layer = geo::FeatureLayers::Layer;
    using LayerMap = std::map<semantic::Class, Layer>;
    using Features = geo::FeatureLayers::Features;

    template <typename Entity>
    void mesh(const Entity &entity, const geometry::Mesh &mesh)
    {
        auto &l(layer(entity.cls));

        for (const auto &sm : geometry::splitById(mesh)) {
            // TODO: get more properties from the source
            Features::Properties props;
            props["material"] = materials_[sm.faces.front().imageId];

            // add surface
            auto &s(l.features.addSurface(++fid_, entity.id, props));
            s.vertices = sm.vertices;
            for (const auto &face : sm.faces) {
                s.surface.emplace_back(face.a, face.b, face.c);
            }
        }
    }

    Layer& layer(semantic::Class cls) {
        auto flayers(layers_.find(cls));
        if (flayers != layers_.end()) { return flayers->second; }

        const auto name(boost::lexical_cast<std::string>(cls));
        return layers_.emplace
            (std::piecewise_construct
             , std::forward_as_tuple(cls)
             , std::forward_as_tuple(name, world_.srs, true))
            .first->second;
    }

    const semantic::World &world_;
    LayerMap layers_;
    Features::Fid fid_;
    std::vector<std::string> materials_;
};

} // namespace

void GeodataSemanticTiled::prepare_impl(Arsenal&)
{
    LOG(info2) << "Preparing <" << id() << ">.";

    const auto &r(resource());

    // try to open datasets
    geo::GeoDataset::open(dem_.dataset);
    geo::GeoDataset::open(dem_.dataset + ".min");
    geo::GeoDataset::open(dem_.dataset + ".max");

    // prepare tile index
    {
        vts::tileset::Index index(referenceFrame().metaBinaryOrder);
        prepareTileIndex(index
                         , (absoluteDataset(definition_.dem.dataset)
                            + "/tiling." + r.id.referenceFrame)
                         , r);

        // save it all
        vts::tileset::saveTileSetIndex(index, root() / "tileset.index");

        const auto deliveryIndexPath(root() / "delivery.index");
        // convert it to delivery index (using a temporary file)
        const auto tmpPath(utility::addExtension
                           (deliveryIndexPath, ".tmp"));
        mmapped::TileIndex::write(tmpPath, index.tileIndex);
        fs::rename(tmpPath, deliveryIndexPath);

        index_ = boost::in_place(referenceFrame().metaBinaryOrder
                                 , deliveryIndexPath);
    }
}

vr::FreeLayer GeodataSemanticTiled::freeLayer(ResourceRoot root) const
{
    const auto &res(resource());

    vr::FreeLayer fl;
    fl.id = res.id.fullId();
    fl.type = vr::FreeLayer::Type::geodataTiles;

    auto &def(fl.createDefinition<vr::FreeLayer::GeodataTiles>());
    def.metaUrl = prependRoot
        (utility::format("{lod}-{x}-{y}.meta?gr=%d%s"
                         , vts::MetaTile::currentVersion()
                         , RevisionWrapper(res.revision, "&"))
         , resource(), root);
    def.geodataUrl = prependRoot
        (utility::format("{lod}-{x}-{y}.geo%s"
                         , RevisionWrapper(res.revision, "?"))
         , resource(), root);
    def.style = styleUrl_;

    def.displaySize = definition_.displaySize;
    def.lodRange = res.lodRange;
    def.tileRange = res.tileRange;
    fl.credits = asInlineCredits(res);
    def.options = definition_.options;

    // done
    return fl;
}

vts::MapConfig GeodataSemanticTiled::mapConfig_impl(ResourceRoot root) const
{
    const auto &res(resource());

    vts::MapConfig mapConfig;
    mapConfig.referenceFrame = *res.referenceFrame;
    mapConfig.srs = vr::listSrs(*res.referenceFrame);

    // add free layer into list of free layers
    mapConfig.freeLayers.add
        (vr::FreeLayer
         (res.id.fullId()
          , prependRoot(std::string("freelayer.json"), resource(), root)));

    // add free layer into view
    mapConfig.view.freeLayers[res.id.fullId()];

    if (definition_.introspection.surface) {
        if (auto other = otherGenerator
            (Resource::Generator::Type::surface
             , addReferenceFrame(*definition_.introspection.surface
                                 , referenceFrameId())))
        {
            mapConfig.merge(other->mapConfig
                            (resolveRoot(resource(), other->resource())));
        }
    }

    // override position
    if (definition_.introspection.position) {
        // user supplied
        mapConfig.position = *definition_.introspection.position;
    } else {
        // calculated
        // TODO: store metadata
        // mapConfig.position = metadata_.position;
    }

    // browser options (must be Json::Value!); overrides browser options from
    // surface's introspection
    if (!definition_.introspection.browserOptions.empty()) {
        mapConfig.browserOptions = definition_.introspection.browserOptions;
    }

    // done
    return mapConfig;
}

Generator::Task
GeodataSemanticTiled::generateFile_impl(const FileInfo &fileInfo
                                        , Sink &sink) const
{
    GeodataFileInfo fi(fileInfo, true, definition_.format);

    // check for valid tileId
    switch (fi.type) {
    case GeodataFileInfo::Type::geo:
        return[=](Sink &sink, Arsenal &arsenal) {
            generateGeodata(sink, fi, arsenal);
        };
        break;

    case GeodataFileInfo::Type::metatile:
        {
        return[=](Sink &sink, Arsenal &arsenal) {
            generateMetatile(sink, fi, arsenal);
        };
        }

    case GeodataFileInfo::Type::config: {
        std::ostringstream os;
        mapConfig(os, ResourceRoot::none);
        sink.content(os.str(), fi.sinkFileInfo());
        break;
    }

    case GeodataFileInfo::Type::definition: {
        std::ostringstream os;
        vr::saveFreeLayer(os, freeLayer(ResourceRoot::none));
        sink.content(os.str(), fi.sinkFileInfo());
        break;
    }

    case GeodataFileInfo::Type::support:
        supportFile(*fi.support, sink, fi.sinkFileInfo());
        break;

    case GeodataFileInfo::Type::registry:
        sink.content(vs::fileIStream
                      (fi.registry->contentType, fi.registry->path)
                     , FileClass::registry);
        break;

    case GeodataFileInfo::Type::style:
        if (stylePath_.empty()) {
            // return internal file
            supportFile(files::defaultMeshStyle, sink, fi.sinkFileInfo());
        } else {
            // return external file
            sink.content(vs::fileIStream
                         (files::defaultMeshStyle.contentType, stylePath_)
                         , FileClass::config);
        }
        break;

    default:
        sink.error(utility::makeError<NotFound>("Not Found."));
        break;
    }

    return {};
}

void GeodataSemanticTiled::generateMetatile(Sink &sink
                                            , const GeodataFileInfo &fi
                                            , Arsenal &arsenal) const
{
    sink.checkAborted();

    if (!index_->meta(fi.tileId)) {
        sink.error(utility::makeError<NotFound>("Metatile not found."));
        return;
    }

    auto metatile(metatileFromDem
                  (fi.tileId, sink, arsenal, resource()
                   , index_->tileIndex, dem_.dataset
                   , dem_.geoidGrid
                   , MaskTree(), definition_.displaySize));

    // write metatile to stream
    std::ostringstream os;
    metatile.save(os);
    sink.content(os.str(), fi.sinkFileInfo());
}

class SemanticRequest : public CustomRequest {
public:
    SemanticRequest(const CustomRequestParams &p)
        : CustomRequest(p.sm)
    {}

    ~SemanticRequest() {}

    virtual void process(bi::interprocess_mutex &mutex, DatasetCache &cache) {
        LOG(info4) << "Semantic: process.";
        (void) mutex;
        (void) cache;
    }

    virtual void consume(Lock &lock, const std::exception_ptr &err) {
        (void) lock;
        (void) err;
        LOG(info4) << "Semantic: consume.";
    }

private:
};

void GeodataSemanticTiled::generateGeodata(Sink &sink
                                           , const GeodataFileInfo &fi
                                           , Arsenal &arsenal) const
{
    (void) fi;

    arsenal.warper.custom([&](const CustomRequestParams &params) {
            return params.sm.construct<SemanticRequest>
                (bi::anonymous_instance)(params);
        }, sink);
}

} // namespace generator
