#pragma once

#include <mfapi.h>

typedef void(*IMAGE_TRANSFORM_FN)(
    BYTE*       pDest,
    LONG        lDestStride,
    const BYTE* pSrc,
    LONG        lSrcStride,
    DWORD       dwWidthInPixels,
    DWORD       dwHeightInPixels
    );


struct ConversionFunction
{
    GUID               subtype;
    IMAGE_TRANSFORM_FN xform;
};

class CColorConverter
{
public:
    CColorConverter();
    ~CColorConverter();

    HRESULT SetConversionFunction(REFGUID subtype);
    void ConvertImageToRGB32(BYTE* pDest, LONG destStride, IMFMediaBuffer *buf);
    bool IsFormatSupported(REFGUID subtype) const;
    HRESULT GetFormat(DWORD index, GUID *pSubtype) const;
    HRESULT SetVideoType(IMFMediaType *pType);

    UINT                    m_width;
    UINT                    m_height;
    LONG                    m_lDefaultStride;
    MFRatio                 m_PixelAR;
    MFVideoInterlaceMode    m_interlace;

protected:
    IMAGE_TRANSFORM_FN      m_convertFn;

    static void TransformImage_RGB24(
        BYTE*       pDest,
        LONG        lDestStride,
        const BYTE* pSrc,
        LONG        lSrcStride,
        DWORD       dwWidthInPixels,
        DWORD       dwHeightInPixels
        );

    static void TransformImage_RGB32(
        BYTE*       pDest,
        LONG        lDestStride,
        const BYTE* pSrc,
        LONG        lSrcStride,
        DWORD       dwWidthInPixels,
        DWORD       dwHeightInPixels
        );

    static void TransformImage_YUY2(
        BYTE*       pDest,
        LONG        lDestStride,
        const BYTE* pSrc,
        LONG        lSrcStride,
        DWORD       dwWidthInPixels,
        DWORD       dwHeightInPixels
        );

    static void TransformImage_NV12(
        BYTE* pDst,
        LONG dstStride,
        const BYTE* pSrc,
        LONG srcStride,
        DWORD dwWidthInPixels,
        DWORD dwHeightInPixels
        );

    static const ConversionFunction s_FormatConversions[];
    static const int s_NumConversionFuncs;

    HRESULT GetDefaultStride(IMFMediaType *pType, LONG *plStride);
};

