/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkColorSpace_DEFINED
#define SkColorSpace_DEFINED

#include "../private/SkOnce.h"
#include "SkMatrix44.h"
#include "SkRefCnt.h"

class SkData;
struct skcms_ICCProfile;

enum SkGammaNamed {
    kLinear_SkGammaNamed,
    kSRGB_SkGammaNamed,
    k2Dot2Curve_SkGammaNamed,
    kNonStandard_SkGammaNamed,
};

/**
 *  Describes a color gamut with primaries and a white point.
 */
struct SK_API SkColorSpacePrimaries {
    float fRX;
    float fRY;
    float fGX;
    float fGY;
    float fBX;
    float fBY;
    float fWX;
    float fWY;

    /**
     *  Convert primaries and a white point to a toXYZD50 matrix, the preferred color gamut
     *  representation of SkColorSpace.
     */
    bool toXYZD50(SkMatrix44* toXYZD50) const;
};

/**
 *  Contains the coefficients for a common transfer function equation, specified as
 *  a transformation from a curved space to linear.
 *
 *  LinearVal = C*InputVal + F        , for 0.0f <= InputVal <  D
 *  LinearVal = (A*InputVal + B)^G + E, for D    <= InputVal <= 1.0f
 *
 *  Function is undefined if InputVal is not in [ 0.0f, 1.0f ].
 *  Resulting LinearVals must be in [ 0.0f, 1.0f ].
 *  Function must be positive and increasing.
 */
struct SK_API SkColorSpaceTransferFn {
    float fG;
    float fA;
    float fB;
    float fC;
    float fD;
    float fE;
    float fF;

    /**
     * Produces a new parametric transfer function equation that is the mathematical inverse of
     * this one.
     */
    SkColorSpaceTransferFn invert() const;

    /**
     * Transform a single float by this transfer function.
     * For negative inputs, returns sign(x) * f(abs(x)).
     */
    float operator()(float x) const {
        SkScalar s = SkScalarSignAsScalar(x);
        x = sk_float_abs(x);
        if (x >= fD) {
            return s * (powf(fA * x + fB, fG) + fE);
        } else {
            return s * (fC * x + fF);
        }
    }
};

class SK_API SkColorSpace : public SkNVRefCnt<SkColorSpace> {
public:
    /**
     *  Create the sRGB color space.
     */
    static sk_sp<SkColorSpace> MakeSRGB();

    /**
     *  Colorspace with the sRGB primaries, but a linear (1.0) gamma. Commonly used for
     *  half-float surfaces, and high precision individual colors (gradient stops, etc...)
     */
    static sk_sp<SkColorSpace> MakeSRGBLinear();

    enum RenderTargetGamma : uint8_t {
        kLinear_RenderTargetGamma,

        /**
         *  Transfer function is the canonical sRGB curve, which has a short linear segment
         *  followed by a 2.4f exponential.
         */
        kSRGB_RenderTargetGamma,
    };

    enum Gamut {
        kSRGB_Gamut,
        kAdobeRGB_Gamut,
        kDCIP3_D65_Gamut,
        kRec2020_Gamut,
    };

    /**
     *  Create an SkColorSpace from a transfer function and a color gamut.
     *
     *  Transfer function can be specified as an enum or as the coefficients to an equation.
     *  Gamut can be specified as an enum or as the matrix transformation to XYZ D50.
     */
    static sk_sp<SkColorSpace> MakeRGB(RenderTargetGamma gamma, Gamut gamut);
    static sk_sp<SkColorSpace> MakeRGB(RenderTargetGamma gamma, const SkMatrix44& toXYZD50);
    static sk_sp<SkColorSpace> MakeRGB(const SkColorSpaceTransferFn& coeffs, Gamut gamut);
    static sk_sp<SkColorSpace> MakeRGB(const SkColorSpaceTransferFn& coeffs,
                                       const SkMatrix44& toXYZD50);

    static sk_sp<SkColorSpace> MakeRGB(SkGammaNamed gammaNamed, const SkMatrix44& toXYZD50);

    /**
     *  Create an SkColorSpace from a parsed (skcms) ICC profile.
     */
    static sk_sp<SkColorSpace> Make(const skcms_ICCProfile&);

    /**
     *  Convert this color space to an skcms ICC profile struct.
     */
    void toProfile(skcms_ICCProfile*) const;

    SkGammaNamed gammaNamed() const { return fGammaNamed; }

    /**
     *  Returns true if the color space gamma is near enough to be approximated as sRGB.
     */
    bool gammaCloseToSRGB() const { return kSRGB_SkGammaNamed == fGammaNamed; }

    /**
     *  Returns true if the color space gamma is linear.
     */
    bool gammaIsLinear() const { return kLinear_SkGammaNamed == fGammaNamed; }

    /**
     *  If the transfer function can be represented as coefficients to the standard
     *  equation, returns true and sets |fn| to the proper values.
     *
     *  If not, returns false.
     */
    bool isNumericalTransferFn(SkColorSpaceTransferFn* fn) const;

    /**
     *  Returns true and sets |toXYZD50| if the color gamut can be described as a matrix.
     *  Returns false otherwise.
     */
    bool toXYZD50(SkMatrix44* toXYZD50) const;

    /**
     *  Describes color space gamut as a transformation to XYZ D50.
     *  Returns nullptr if color gamut cannot be described in terms of XYZ D50.
     */
    const SkMatrix44* toXYZD50() const { return &fToXYZD50; }

    /**
     *  Describes color space gamut as a transformation from XYZ D50
     *  Returns nullptr if color gamut cannot be described in terms of XYZ D50.
     */
    const SkMatrix44* fromXYZD50() const;

    /**
     *  Returns a hash of the gamut transofmration to XYZ D50. Allows for fast equality checking
     *  of gamuts, at the (very small) risk of collision.
     *  Returns 0 if color gamut cannot be described in terms of XYZ D50.
     */
    uint32_t toXYZD50Hash() const { return fToXYZD50Hash; }

    /**
     *  Returns a color space with the same gamut as this one, but with a linear gamma.
     *  For color spaces whose gamut can not be described in terms of XYZ D50, returns
     *  linear sRGB.
     */
    sk_sp<SkColorSpace> makeLinearGamma() const;

    /**
     *  Returns a color space with the same gamut as this one, with with the sRGB transfer
     *  function. For color spaces whose gamut can not be described in terms of XYZ D50, returns
     *  sRGB.
     */
    sk_sp<SkColorSpace> makeSRGBGamma() const;

    /**
     *  Returns a color space with the same transfer function as this one, but with the primary
     *  colors rotated. For any XYZ space, this produces a new color space that maps RGB to GBR
     *  (when applied to a source), and maps RGB to BRG (when applied to a destination). For other
     *  types of color spaces, returns nullptr.
     *
     *  This is used for testing, to construct color spaces that have severe and testable behavior.
     */
    sk_sp<SkColorSpace> makeColorSpin() const;

    /**
     *  Returns true if the color space is sRGB.
     *  Returns false otherwise.
     *
     *  This allows a little bit of tolerance, given that we might see small numerical error
     *  in some cases: converting ICC fixed point to float, converting white point to D50,
     *  rounding decisions on transfer function and matrix.
     *
     *  This does not consider a 2.2f exponential transfer function to be sRGB.  While these
     *  functions are similar (and it is sometimes useful to consider them together), this
     *  function checks for logical equality.
     */
    bool isSRGB() const;

    /**
     *  Returns nullptr on failure.  Fails when we fallback to serializing ICC data and
     *  the data is too large to serialize.
     */
    sk_sp<SkData> serialize() const;

    /**
     *  If |memory| is nullptr, returns the size required to serialize.
     *  Otherwise, serializes into |memory| and returns the size.
     */
    size_t writeToMemory(void* memory) const;

    static sk_sp<SkColorSpace> Deserialize(const void* data, size_t length);

    /**
     *  If both are null, we return true.  If one is null and the other is not, we return false.
     *  If both are non-null, we do a deeper compare.
     */
    static bool Equals(const SkColorSpace* src, const SkColorSpace* dst);

    void       transferFn(float gabcdef[7]) const;
    void    invTransferFn(float gabcdef[7]) const;
    void gamutTransformTo(const SkColorSpace* dst, float src_to_dst_row_major[9]) const;

private:
    friend class SkColorSpaceSingletonFactory;

    SkColorSpace(SkGammaNamed gammaNamed,
                 const float transferFn[7],
                 const SkMatrix44& toXYZ);

    void computeLazyDstFields() const;

    SkGammaNamed           fGammaNamed;         // TODO: 2-bit, pack more tightly?  or drop?
    uint32_t               fToXYZD50Hash;       // TODO: Switch to whole-CS hash?

    float                  fTransferFn[7];
    SkMatrix44             fToXYZD50;           // TODO: Drop
    float                  fToXYZD50_3x3[9];    // row-major

    mutable float          fInvTransferFn[7];
    mutable SkMatrix44     fFromXYZD50;         // TODO: Drop
    mutable float          fFromXYZD50_3x3[9];  // row-major
    mutable SkOnce         fLazyDstFieldsOnce;
};

#endif
