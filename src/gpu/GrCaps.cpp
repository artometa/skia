/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrCaps.h"

#include "GrBackendSurface.h"
#include "GrContextOptions.h"
#include "GrWindowRectangles.h"
#include "SkJSONWriter.h"

static const char* pixel_config_name(GrPixelConfig config) {
    switch (config) {
        case kUnknown_GrPixelConfig: return "Unknown";
        case kAlpha_8_GrPixelConfig: return "Alpha8";
        case kAlpha_8_as_Alpha_GrPixelConfig: return "Alpha8_asAlpha";
        case kAlpha_8_as_Red_GrPixelConfig: return "Alpha8_asRed";
        case kGray_8_GrPixelConfig: return "Gray8";
        case kGray_8_as_Lum_GrPixelConfig: return "Gray8_asLum";
        case kGray_8_as_Red_GrPixelConfig: return "Gray8_asRed";
        case kRGB_565_GrPixelConfig: return "RGB565";
        case kRGBA_4444_GrPixelConfig: return "RGBA444";
        case kRGBA_8888_GrPixelConfig: return "RGBA8888";
        case kRGB_888_GrPixelConfig: return "RGB888";
        case kBGRA_8888_GrPixelConfig: return "BGRA8888";
        case kSRGBA_8888_GrPixelConfig: return "SRGBA8888";
        case kSBGRA_8888_GrPixelConfig: return "SBGRA8888";
        case kRGBA_1010102_GrPixelConfig: return "RGBA1010102";
        case kRGBA_float_GrPixelConfig: return "RGBAFloat";
        case kRG_float_GrPixelConfig: return "RGFloat";
        case kAlpha_half_GrPixelConfig: return "AlphaHalf";
        case kAlpha_half_as_Red_GrPixelConfig: return "AlphaHalf_asRed";
        case kRGBA_half_GrPixelConfig: return "RGBAHalf";
    }
    SK_ABORT("Invalid pixel config");
    return "<invalid>";
}

GrCaps::GrCaps(const GrContextOptions& options) {
    fMipMapSupport = false;
    fNPOTTextureTileSupport = false;
    fSRGBSupport = false;
    fSRGBWriteControl = false;
    fDiscardRenderTargetSupport = false;
    fReuseScratchTextures = true;
    fReuseScratchBuffers = true;
    fGpuTracingSupport = false;
    fOversizedStencilSupport = false;
    fTextureBarrierSupport = false;
    fSampleLocationsSupport = false;
    fMultisampleDisableSupport = false;
    fInstanceAttribSupport = false;
    fUsesMixedSamples = false;
    fUsePrimitiveRestart = false;
    fPreferClientSideDynamicBuffers = false;
    fPreferFullscreenClears = false;
    fMustClearUploadedBufferData = false;
    fSupportsAHardwareBufferImages = false;
    fSampleShadingSupport = false;
    fFenceSyncSupport = false;
    fCrossContextTextureSupport = false;
    fHalfFloatVertexAttributeSupport = false;
    fDynamicStateArrayGeometryProcessorTextureSupport = false;

    fBlendEquationSupport = kBasic_BlendEquationSupport;
    fAdvBlendEqBlacklist = 0;

    fMapBufferFlags = kNone_MapFlags;

    fMaxVertexAttributes = 0;
    fMaxRenderTargetSize = 1;
    fMaxPreferredRenderTargetSize = 1;
    fMaxTextureSize = 1;
    fMaxRasterSamples = 0;
    fMaxWindowRectangles = 0;

    // An default count of 4 was chosen because of the common pattern in Blink of:
    //   isect RR
    //   diff  RR
    //   isect convex_poly
    //   isect convex_poly
    // when drawing rounded div borders.
    fMaxClipAnalyticFPs = 4;

    fSuppressPrints = options.fSuppressPrints;
#if GR_TEST_UTILS
    fWireframeMode = options.fWireframeMode;
#else
    fWireframeMode = false;
#endif
    fBufferMapThreshold = options.fBufferMapThreshold;
    fBlacklistCoverageCounting = false;
    fAvoidStencilBuffers = false;
    fAvoidWritePixelsFastPath = false;

    fPreferVRAMUseOverFlushes = true;

    fDriverBugWorkarounds = options.fDriverBugWorkarounds;
}

void GrCaps::applyOptionsOverrides(const GrContextOptions& options) {
    this->onApplyOptionsOverrides(options);
    if (options.fDisableDriverCorrectnessWorkarounds) {
        // We always blacklist coverage counting on Vulkan currently. TODO: Either stop doing that
        // or disambiguate blacklisting from incomplete implementation.
        // SkASSERT(!fBlacklistCoverageCounting);
        SkASSERT(!fAvoidStencilBuffers);
        SkASSERT(!fAdvBlendEqBlacklist);
    }

    fMaxTextureSize = SkTMin(fMaxTextureSize, options.fMaxTextureSizeOverride);
    fMaxTileSize = fMaxTextureSize;
#if GR_TEST_UTILS
    // If the max tile override is zero, it means we should use the max texture size.
    if (options.fMaxTileSizeOverride && options.fMaxTileSizeOverride < fMaxTextureSize) {
        fMaxTileSize = options.fMaxTileSizeOverride;
    }
    if (options.fSuppressGeometryShaders) {
        fShaderCaps->fGeometryShaderSupport = false;
    }
#endif
    if (fMaxWindowRectangles > GrWindowRectangles::kMaxWindows) {
        SkDebugf("WARNING: capping window rectangles at %i. HW advertises support for %i.\n",
                 GrWindowRectangles::kMaxWindows, fMaxWindowRectangles);
        fMaxWindowRectangles = GrWindowRectangles::kMaxWindows;
    }
    fAvoidStencilBuffers = options.fAvoidStencilBuffers;

    fDriverBugWorkarounds.applyOverrides(options.fDriverBugWorkarounds);
}

static SkString map_flags_to_string(uint32_t flags) {
    SkString str;
    if (GrCaps::kNone_MapFlags == flags) {
        str = "none";
    } else {
        SkASSERT(GrCaps::kCanMap_MapFlag & flags);
        SkDEBUGCODE(flags &= ~GrCaps::kCanMap_MapFlag);
        str = "can_map";

        if (GrCaps::kSubset_MapFlag & flags) {
            str.append(" partial");
        } else {
            str.append(" full");
        }
        SkDEBUGCODE(flags &= ~GrCaps::kSubset_MapFlag);
    }
    SkASSERT(0 == flags); // Make sure we handled all the flags.
    return str;
}

void GrCaps::dumpJSON(SkJSONWriter* writer) const {
    writer->beginObject();

    writer->appendBool("MIP Map Support", fMipMapSupport);
    writer->appendBool("NPOT Texture Tile Support", fNPOTTextureTileSupport);
    writer->appendBool("sRGB Support", fSRGBSupport);
    writer->appendBool("sRGB Write Control", fSRGBWriteControl);
    writer->appendBool("Discard Render Target Support", fDiscardRenderTargetSupport);
    writer->appendBool("Reuse Scratch Textures", fReuseScratchTextures);
    writer->appendBool("Reuse Scratch Buffers", fReuseScratchBuffers);
    writer->appendBool("Gpu Tracing Support", fGpuTracingSupport);
    writer->appendBool("Oversized Stencil Support", fOversizedStencilSupport);
    writer->appendBool("Texture Barrier Support", fTextureBarrierSupport);
    writer->appendBool("Sample Locations Support", fSampleLocationsSupport);
    writer->appendBool("Multisample disable support", fMultisampleDisableSupport);
    writer->appendBool("Instance Attrib Support", fInstanceAttribSupport);
    writer->appendBool("Uses Mixed Samples", fUsesMixedSamples);
    writer->appendBool("Use primitive restart", fUsePrimitiveRestart);
    writer->appendBool("Prefer client-side dynamic buffers", fPreferClientSideDynamicBuffers);
    writer->appendBool("Prefer fullscreen clears", fPreferFullscreenClears);
    writer->appendBool("Must clear buffer memory", fMustClearUploadedBufferData);
    writer->appendBool("Supports importing AHardwareBuffers", fSupportsAHardwareBufferImages);
    writer->appendBool("Sample shading support", fSampleShadingSupport);
    writer->appendBool("Fence sync support", fFenceSyncSupport);
    writer->appendBool("Cross context texture support", fCrossContextTextureSupport);
    writer->appendBool("Half float vertex attribute support", fHalfFloatVertexAttributeSupport);
    writer->appendBool("Specify GeometryProcessor textures as a dynamic state array",
                       fDynamicStateArrayGeometryProcessorTextureSupport);

    writer->appendBool("Blacklist Coverage Counting Path Renderer [workaround]",
                       fBlacklistCoverageCounting);
    writer->appendBool("Prefer VRAM Use over flushes [workaround]", fPreferVRAMUseOverFlushes);
    writer->appendBool("Avoid stencil buffers [workaround]", fAvoidStencilBuffers);

    if (this->advancedBlendEquationSupport()) {
        writer->appendHexU32("Advanced Blend Equation Blacklist", fAdvBlendEqBlacklist);
    }

    writer->appendS32("Max Vertex Attributes", fMaxVertexAttributes);
    writer->appendS32("Max Texture Size", fMaxTextureSize);
    writer->appendS32("Max Render Target Size", fMaxRenderTargetSize);
    writer->appendS32("Max Preferred Render Target Size", fMaxPreferredRenderTargetSize);
    writer->appendS32("Max Raster Samples", fMaxRasterSamples);
    writer->appendS32("Max Window Rectangles", fMaxWindowRectangles);
    writer->appendS32("Max Clip Analytic Fragment Processors", fMaxClipAnalyticFPs);

    static const char* kBlendEquationSupportNames[] = {
        "Basic",
        "Advanced",
        "Advanced Coherent",
    };
    GR_STATIC_ASSERT(0 == kBasic_BlendEquationSupport);
    GR_STATIC_ASSERT(1 == kAdvanced_BlendEquationSupport);
    GR_STATIC_ASSERT(2 == kAdvancedCoherent_BlendEquationSupport);
    GR_STATIC_ASSERT(SK_ARRAY_COUNT(kBlendEquationSupportNames) == kLast_BlendEquationSupport + 1);

    writer->appendString("Blend Equation Support",
                         kBlendEquationSupportNames[fBlendEquationSupport]);
    writer->appendString("Map Buffer Support", map_flags_to_string(fMapBufferFlags).c_str());

    SkASSERT(!this->isConfigRenderable(kUnknown_GrPixelConfig));
    SkASSERT(!this->isConfigTexturable(kUnknown_GrPixelConfig));

    writer->beginArray("configs");

    for (size_t i = 1; i < kGrPixelConfigCnt; ++i) {
        GrPixelConfig config = static_cast<GrPixelConfig>(i);
        writer->beginObject(nullptr, false);
        writer->appendString("name", pixel_config_name(config));
        writer->appendS32("max sample count", this->maxRenderTargetSampleCount(config));
        writer->appendBool("texturable", this->isConfigTexturable(config));
        writer->endObject();
    }

    writer->endArray();

    this->onDumpJSON(writer);

    writer->appendName("shaderCaps");
    this->shaderCaps()->dumpJSON(writer);

    writer->endObject();
}

bool GrCaps::validateSurfaceDesc(const GrSurfaceDesc& desc, GrMipMapped mipped) const {
    if (!this->isConfigTexturable(desc.fConfig)) {
        return false;
    }

    if (GrMipMapped::kYes == mipped && !this->mipMapSupport()) {
        return false;
    }

    if (desc.fWidth < 1 || desc.fHeight < 1) {
        return false;
    }

    if (SkToBool(desc.fFlags & kRenderTarget_GrSurfaceFlag)) {
        if (0 == this->getRenderTargetSampleCount(desc.fSampleCnt, desc.fConfig)) {
            return false;
        }
        int maxRTSize = this->maxRenderTargetSize();
        if (desc.fWidth > maxRTSize || desc.fHeight > maxRTSize) {
            return false;
        }
    } else {
        // We currently do not support multisampled textures
        if (desc.fSampleCnt > 1) {
            return false;
        }
        int maxSize = this->maxTextureSize();
        if (desc.fWidth > maxSize || desc.fHeight > maxSize) {
            return false;
        }
    }

    return true;
}

GrBackendFormat GrCaps::createFormatFromBackendTexture(const GrBackendTexture& backendTex) const {
    if (!backendTex.isValid()) {
        return GrBackendFormat();
    }
    return this->onCreateFormatFromBackendTexture(backendTex);
}
