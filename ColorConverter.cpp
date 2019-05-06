#include "ColorConverter.h"

#include <d3d9.h>
#include <mferror.h>
#include "BufferLock.h"

const ConversionFunction CColorConverter::s_FormatConversions[] =
{
    { MFVideoFormat_RGB32, CColorConverter::TransformImage_RGB32 },
    { MFVideoFormat_RGB24, CColorConverter::TransformImage_RGB24 },
    { MFVideoFormat_YUY2, CColorConverter::TransformImage_YUY2 },
    { MFVideoFormat_NV12, CColorConverter::TransformImage_NV12 }
};

const int CColorConverter::s_NumConversionFuncs = ARRAYSIZE(s_FormatConversions);

CColorConverter::CColorConverter()
{
    m_convertFn = NULL;

}


CColorConverter::~CColorConverter()
{

}

//-------------------------------------------------------------------
//
// Conversion functions
//
//-------------------------------------------------------------------

__forceinline BYTE Clip(int clr)
{
    return (BYTE)(clr < 0 ? 0 : (clr > 255 ? 255 : clr));
}

__forceinline RGBQUAD ConvertYCrCbToRGB(
    int y,
    int cr,
    int cb
    )
{
    RGBQUAD rgbq;

    int c = y - 16;
    int d = cb - 128;
    int e = cr - 128;

    rgbq.rgbRed = Clip((298 * c + 409 * e + 128) >> 8);
    rgbq.rgbGreen = Clip((298 * c - 100 * d - 208 * e + 128) >> 8);
    rgbq.rgbBlue = Clip((298 * c + 516 * d + 128) >> 8);

    return rgbq;
}

//-------------------------------------------------------------------
// TransformImage_RGB24 
//
// RGB-24 to RGB-32
//-------------------------------------------------------------------

void CColorConverter::TransformImage_RGB24(
    BYTE*       pDest,
    LONG        lDestStride,
    const BYTE* pSrc,
    LONG        lSrcStride,
    DWORD       dwWidthInPixels,
    DWORD       dwHeightInPixels
    )
{
    for(DWORD y = 0; y < dwHeightInPixels; y++)
    {
        RGBTRIPLE *pSrcPel = (RGBTRIPLE*)pSrc;
        DWORD *pDestPel = (DWORD*)pDest;

        for(DWORD x = 0; x < dwWidthInPixels; x++)
        {
            pDestPel[x] = D3DCOLOR_XRGB(
                pSrcPel[x].rgbtRed,
                pSrcPel[x].rgbtGreen,
                pSrcPel[x].rgbtBlue
                );
        }

        pSrc += lSrcStride;
        pDest += lDestStride;
    }
}

//-------------------------------------------------------------------
// TransformImage_RGB32
//
// RGB-32 to RGB-32 
//
// Note: This function is needed to copy the image from system
// memory to the Direct3D surface.
//-------------------------------------------------------------------

void CColorConverter::TransformImage_RGB32(
    BYTE*       pDest,
    LONG        lDestStride,
    const BYTE* pSrc,
    LONG        lSrcStride,
    DWORD       dwWidthInPixels,
    DWORD       dwHeightInPixels
    )
{
    MFCopyImage(pDest, lDestStride, pSrc, lSrcStride, dwWidthInPixels * 4, dwHeightInPixels);
}

//-------------------------------------------------------------------
// TransformImage_YUY2 
//
// YUY2 to RGB-32
//-------------------------------------------------------------------

void CColorConverter::TransformImage_YUY2(
    BYTE*       pDest,
    LONG        lDestStride,
    const BYTE* pSrc,
    LONG        lSrcStride,
    DWORD       dwWidthInPixels,
    DWORD       dwHeightInPixels
    )
{
    for(DWORD y = 0; y < dwHeightInPixels; y++)
    {
        RGBQUAD *pDestPel = (RGBQUAD*)pDest;
        WORD    *pSrcPel = (WORD*)pSrc;

        for(DWORD x = 0; x < dwWidthInPixels; x += 2)
        {
            // Byte order is U0 Y0 V0 Y1

            int y0 = (int)LOBYTE(pSrcPel[x]);
            int u0 = (int)HIBYTE(pSrcPel[x]);
            int y1 = (int)LOBYTE(pSrcPel[x + 1]);
            int v0 = (int)HIBYTE(pSrcPel[x + 1]);

            pDestPel[x] = ConvertYCrCbToRGB(y0, v0, u0);
            pDestPel[x + 1] = ConvertYCrCbToRGB(y1, v0, u0);
        }

        pSrc += lSrcStride;
        pDest += lDestStride;
    }

}


//-------------------------------------------------------------------
// TransformImage_NV12
//
// NV12 to RGB-32
//-------------------------------------------------------------------

void CColorConverter::TransformImage_NV12(
    BYTE* pDst,
    LONG dstStride,
    const BYTE* pSrc,
    LONG srcStride,
    DWORD dwWidthInPixels,
    DWORD dwHeightInPixels
    )
{
    const BYTE* lpBitsY = pSrc;
    const BYTE* lpBitsCb = lpBitsY + (dwHeightInPixels * srcStride);;
    const BYTE* lpBitsCr = lpBitsCb + 1;

    for(UINT y = 0; y < dwHeightInPixels; y += 2)
    {
        const BYTE* lpLineY1 = lpBitsY;
        const BYTE* lpLineY2 = lpBitsY + srcStride;
        const BYTE* lpLineCr = lpBitsCr;
        const BYTE* lpLineCb = lpBitsCb;

        LPBYTE lpDibLine1 = pDst;
        LPBYTE lpDibLine2 = pDst + dstStride;

        for(UINT x = 0; x < dwWidthInPixels; x += 2)
        {
            int  y0 = (int)lpLineY1[0];
            int  y1 = (int)lpLineY1[1];
            int  y2 = (int)lpLineY2[0];
            int  y3 = (int)lpLineY2[1];
            int  cb = (int)lpLineCb[0];
            int  cr = (int)lpLineCr[0];

            RGBQUAD r = ConvertYCrCbToRGB(y0, cr, cb);
            lpDibLine1[0] = r.rgbBlue;
            lpDibLine1[1] = r.rgbGreen;
            lpDibLine1[2] = r.rgbRed;
            lpDibLine1[3] = 0; // Alpha

            r = ConvertYCrCbToRGB(y1, cr, cb);
            lpDibLine1[4] = r.rgbBlue;
            lpDibLine1[5] = r.rgbGreen;
            lpDibLine1[6] = r.rgbRed;
            lpDibLine1[7] = 0; // Alpha

            r = ConvertYCrCbToRGB(y2, cr, cb);
            lpDibLine2[0] = r.rgbBlue;
            lpDibLine2[1] = r.rgbGreen;
            lpDibLine2[2] = r.rgbRed;
            lpDibLine2[3] = 0; // Alpha

            r = ConvertYCrCbToRGB(y3, cr, cb);
            lpDibLine2[4] = r.rgbBlue;
            lpDibLine2[5] = r.rgbGreen;
            lpDibLine2[6] = r.rgbRed;
            lpDibLine2[7] = 0; // Alpha

            lpLineY1 += 2;
            lpLineY2 += 2;
            lpLineCr += 2;
            lpLineCb += 2;

            lpDibLine1 += 8;
            lpDibLine2 += 8;
        }

        pDst += (2 * dstStride);
        lpBitsY += (2 * srcStride);
        lpBitsCr += srcStride;
        lpBitsCb += srcStride;
    }
}

HRESULT CColorConverter::SetConversionFunction(REFGUID subtype)
{
    m_convertFn = NULL;

    for(DWORD i = 0; i < s_NumConversionFuncs; i++)
    {
        if(s_FormatConversions[i].subtype == subtype)
        {
            m_convertFn = s_FormatConversions[i].xform;
            return S_OK;
        }
    }

    return MF_E_INVALIDMEDIATYPE;
}

void CColorConverter::ConvertImageToRGB32(BYTE* pDest, LONG destStride, IMFMediaBuffer *buf)
{
    BYTE *pbScanline0 = NULL;
    LONG lStride = 0;

    VideoBufferLock buffer(buf);

    HRESULT hr = buffer.LockBuffer(m_lDefaultStride, m_height, &pbScanline0, &lStride);
    if(SUCCEEDED(hr))
    {
        if(m_convertFn)
            m_convertFn(pDest, destStride, pbScanline0, lStride, m_width, m_height);
    }  
}

bool CColorConverter::IsFormatSupported(REFGUID subtype) const
{
    for(DWORD i = 0; i < s_NumConversionFuncs; i++)
    {
        if(subtype == s_FormatConversions[i].subtype)
        {
            return TRUE;
        }
    }
    return FALSE;
}

HRESULT CColorConverter::GetFormat(DWORD index, GUID *pSubtype) const
{
    if(index < s_NumConversionFuncs)
    {
        *pSubtype = s_FormatConversions[index].subtype;
        return S_OK;
    }
    return MF_E_NO_MORE_TYPES;
}

//-------------------------------------------------------------------
// SetVideoType
//
// Set the video format.  
//-------------------------------------------------------------------

HRESULT CColorConverter::SetVideoType(IMFMediaType *pType)
{
    HRESULT hr = S_OK;
    GUID subtype = { 0 };
    MFRatio PAR = { 0 };

    // Find the video subtype.
    hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);

    if(SUCCEEDED(hr))
    {
        // Choose a conversion function.
        // (This also validates the format type.)

        hr = SetConversionFunction(subtype);

        if(SUCCEEDED(hr))
        {
            //
            // Get some video attributes.
            //

            // Get the frame size.
            hr = MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &m_width, &m_height);

            if(SUCCEEDED(hr))
            {
                // Get the interlace mode. Default: assume progressive.
                m_interlace = (MFVideoInterlaceMode)MFGetAttributeUINT32(
                    pType,
                    MF_MT_INTERLACE_MODE,
                    MFVideoInterlace_Progressive
                    );

                // Get the image stride.
                hr = GetDefaultStride(pType, &m_lDefaultStride);

                if(SUCCEEDED(hr))
                {
                    // Get the pixel aspect ratio. Default: Assume square pixels (1:1)
                    hr = MFGetAttributeRatio(
                        pType,
                        MF_MT_PIXEL_ASPECT_RATIO,
                        (UINT32*)&PAR.Numerator,
                        (UINT32*)&PAR.Denominator
                        );

                    if(SUCCEEDED(hr))
                    {
                        m_PixelAR = PAR;
                    }
                    else
                    {
                        m_PixelAR.Numerator = m_PixelAR.Denominator = 1;
                    }
                }
            }
        }
    }

    return hr;
}

//-----------------------------------------------------------------------------
// GetDefaultStride
//
// Gets the default stride for a video frame, assuming no extra padding bytes.
//
//-----------------------------------------------------------------------------

HRESULT CColorConverter::GetDefaultStride(IMFMediaType *pType, LONG *plStride)
{
    LONG lStride = 0;

    // Try to get the default stride from the media type.
    HRESULT hr = pType->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32*)&lStride);
    if(FAILED(hr))
    {
        // Attribute not set. Try to calculate the default stride.
        GUID subtype = GUID_NULL;

        UINT32 width = 0;
        UINT32 height = 0;

        // Get the subtype and the image size.
        hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);
        if(SUCCEEDED(hr))
        {
            hr = MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height);
        }
        if(SUCCEEDED(hr))
        {
            hr = MFGetStrideForBitmapInfoHeader(subtype.Data1, width, &lStride);
        }

        // Set the attribute for later reference.
        if(SUCCEEDED(hr))
        {
            (void)pType->SetUINT32(MF_MT_DEFAULT_STRIDE, UINT32(lStride));
        }
    }

    if(SUCCEEDED(hr))
    {
        *plStride = lStride;
    }
    return hr;
}
