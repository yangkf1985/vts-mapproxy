#include "imgproc/rastermask/cvmat.hpp"

#include "../error.hpp"
#include "./operations.hpp"

namespace {

cv::Mat* allocateMat(ManagedBuffer &mb
                     , const math::Size2 &size, int type)
{
    // calculate sizes
    const auto dataSize(math::area(size) * CV_ELEM_SIZE(type));
    const auto matSize(sizeof(cv::Mat) + dataSize);

    // create raw memory to hold matrix and data
    char *raw(static_cast<char*>(mb.allocate(matSize)));

    // allocate matrix in raw data block
    return new (raw) cv::Mat(size.height, size.width, type
                             , raw + sizeof(cv::Mat));
}

} // namespace

cv::Mat* warpImage(DatasetCache &cache, ManagedBuffer &mb
                   , const std::string &dataset
                   , const boost::optional<std::string> &maskDataset
                   , const geo::SrsDefinition &srs
                   , const math::Extents2 &extents
                   , const math::Size2 &size
                   , geo::GeoDataset::Resampling resampling)
{
    auto &src(cache.dataset(dataset));
    auto dst(geo::GeoDataset::deriveInMemory(src, srs, size, extents));
    src.warpInto(dst, resampling);

    if (dst.cmask().empty()) {
        throw NotFound("No valid data.");
    }

    // apply mask set if defined
    if (maskDataset) {
        auto &srcMask(cache.dataset(*maskDataset));
        auto dstMask(geo::GeoDataset::deriveInMemory
                     (srcMask, srs, size, extents));
        srcMask.warpInto(dstMask, resampling);
        dst.applyMask(dstMask.cmask());
    }

    if (dst.cmask().empty()) {
        throw NotFound("No valid data.");
    }

    // grab destination
    auto dstMat(dst.cdata());
    auto type(CV_MAKETYPE(CV_8U, dstMat.channels()));

    auto *tile(allocateMat(mb, size, type));
    dstMat.convertTo(*tile, type);
    return tile;
}

cv::Mat* warpMask(DatasetCache &cache, ManagedBuffer &mb
                  , const std::string &dataset
                  , const boost::optional<std::string> &maskDataset
                  , const geo::SrsDefinition &srs
                  , const math::Extents2 &extents
                  , const math::Size2 &size
                  , geo::GeoDataset::Resampling resampling)
{
    auto &src(cache.dataset(dataset));
    auto dst(geo::GeoDataset::deriveInMemory(src, srs, size, extents));
    src.warpInto(dst, resampling);

    if (dst.cmask().empty()) {
        throw NotFound("No valid data.");
    }

    // apply mask set if defined
    if (maskDataset) {
        auto &srcMask(cache.dataset(*maskDataset));
        auto dstMask(geo::GeoDataset::deriveInMemory
                     (srcMask, srs, size, extents));
        srcMask.warpInto(dstMask, resampling);
        dst.applyMask(dstMask.cmask());
    }

    if (dst.cmask().empty()) {
        throw NotFound("No valid data.");
    }

    auto &cmask(dst.cmask());
    auto *mask(allocateMat(mb, maskMatSize(cmask), maskMatDataType(cmask)));
    asCvMat(*mask, cmask);
    return mask;
}

cv::Mat* warpDetailMask(DatasetCache &cache, ManagedBuffer &mb
                        , const std::string &dataset
                        , const boost::optional<std::string> &maskDataset
                        , const geo::SrsDefinition &srs
                        , const math::Extents2 &extents
                        , const math::Size2 &size)
{
    if (!maskDataset) {
        (void) dataset;
        throw InternalError
            ("Unimplemented: TODO: generate metatile from data.");
    }

    // generate metatile from mask dataset
    auto &srcMask(cache.mask(*maskDataset));
    auto dstMask(geo::GeoDataset::deriveInMemory
                 (srcMask, srs, size, extents));
    srcMask.warpInto(dstMask, geo::GeoDataset::Resampling::average);

    // mask is guaranteed to have single (double) channel
    auto &dstMat(dstMask.cdata());
    auto *tile(allocateMat(mb, size, dstMat.type()));
    dstMat.copyTo(*tile);
    return tile;
}

cv::Mat* warp(DatasetCache &cache, ManagedBuffer &mb
              , const GdalWarper::RasterRequest &req)
{
    switch (req.operation) {
    case GdalWarper::RasterRequest::Operation::image:
        return warpImage
            (cache, mb, req.dataset, req.mask, req.srs, req.extents, req.size
             , req.resampling);
    case GdalWarper::RasterRequest::Operation::mask:
        return warpMask
            (cache, mb, req.dataset, req.mask, req.srs, req.extents, req.size
             , req.resampling);
    case GdalWarper::RasterRequest::Operation::detailMask:
        return warpDetailMask
            (cache, mb, req.dataset, req.mask, req.srs, req.extents, req.size);
    }
    throw;
}
