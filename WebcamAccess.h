#pragma once

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include "ColorConverter.h"

template <class T> void SafeRelease(T **ppT)
{
    if(*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

struct ChooseDeviceParam
{
    IMFActivate **ppDevices;    // Array of IMFActivate pointers.
    UINT32      count;          // Number of elements in the array.
    UINT32      selection;      // Selected device, by array index.
};

class CWebcamAccess
{
public:
    CWebcamAccess();
    ~CWebcamAccess();

    HRESULT Initialize();
    bool SetDeviceIndex(unsigned int index);
    HRESULT PrepareDevice();

    void GetImageSizes(unsigned int &width, unsigned int&height);
    HRESULT GetImageData(BYTE *buffer, LONG stride);

protected:
    ChooseDeviceParam       m_cam_devices;
    IMFSourceReader         *m_pReader;
    CColorConverter         m_color_converter;
    bool                    m_ready;

    void ShowErrorMessage(PCWSTR format, HRESULT hrErr);
    HRESULT BuildListOfDevices();
    void CloseDevice();
    HRESULT TryMediaType(IMFMediaType *pType);
};

