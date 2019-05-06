#include "WebcamAccess.h"
#include <strsafe.h>
#include "BufferLock.h"

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

void CWebcamAccess::ShowErrorMessage(PCWSTR format, HRESULT hrErr)
{
    HRESULT hr = S_OK;
    WCHAR msg[MAX_PATH];

    hr = StringCbPrintf(msg, sizeof(msg), L"%s (hr=0x%X)", format, hrErr);

    if(SUCCEEDED(hr))
    {
        MessageBox(NULL, msg, L"Error", MB_ICONERROR);
    }
    else
    {
        DebugBreak();
    }
}

CWebcamAccess::CWebcamAccess()
{
    ZeroMemory(&m_cam_devices, sizeof(m_cam_devices));
    m_pReader = NULL;
}


CWebcamAccess::~CWebcamAccess()
{
    CloseDevice();
    for(DWORD i = 0; i < m_cam_devices.count; i++)
    {
        SafeRelease(&m_cam_devices.ppDevices[i]);
    }
    CoTaskMemFree(m_cam_devices.ppDevices);
}

void CWebcamAccess::CloseDevice()
{
    SafeRelease(&m_pReader);
    m_ready = false;
}

HRESULT CWebcamAccess::Initialize()
{
    return BuildListOfDevices();
}

bool CWebcamAccess::SetDeviceIndex(unsigned int index)
{
    if(index < m_cam_devices.count)
    {
        m_cam_devices.selection = index;
        return true;
    }
    return false;
}

HRESULT CWebcamAccess::BuildListOfDevices()
{
    HRESULT hr = S_OK;

    ZeroMemory(&m_cam_devices, sizeof(m_cam_devices));

    IMFAttributes *pAttributes = NULL;

    // Initialize an attribute store to specify enumeration parameters.

    hr = MFCreateAttributes(&pAttributes, 1);

    if(SUCCEEDED(hr))
    {

        // Ask for source type = video capture devices.

        hr = pAttributes->SetGUID(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
            );

        if(SUCCEEDED(hr))
        {
            // Enumerate devices.
            hr = MFEnumDeviceSources(pAttributes, &m_cam_devices.ppDevices, &m_cam_devices.count);
        }
    }

    SafeRelease(&pAttributes);

    if(FAILED(hr))
    {
        ShowErrorMessage(L"Cannot enumerate video capture devices", hr);
    }

    return hr;
}

HRESULT CWebcamAccess::PrepareDevice()
{
    if(m_cam_devices.selection >= m_cam_devices.count)
    {
        MessageBox(NULL, L"Selected device index larger than device count.", L"Error", MB_ICONERROR);
        return false;
    }

    HRESULT hr = S_OK;

    IMFMediaSource  *pSource = NULL;
    IMFAttributes   *pAttributes = NULL;
    IMFMediaType    *pType = NULL;
    IMFActivate *pActivate = m_cam_devices.ppDevices[m_cam_devices.selection];

    // Release the current device, if any.

    CloseDevice();

    m_ready = true;

    // Create the media source for the device.
    if(SUCCEEDED(hr))
    {
        hr = pActivate->ActivateObject(
            __uuidof(IMFMediaSource),
            (void**)&pSource
            );
    }

    //
    // Create the source reader.
    //

    // Create an attribute store to hold initialization settings.

    if(SUCCEEDED(hr))
    {
        hr = MFCreateAttributes(&pAttributes, 2);
    }
    if(SUCCEEDED(hr))
    {
        hr = pAttributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, TRUE);
    }

    if(SUCCEEDED(hr))
    {
        hr = MFCreateSourceReaderFromMediaSource(
            pSource,
            pAttributes,
            &m_pReader
            );
    }

    // Try to find a suitable output type.
    if(SUCCEEDED(hr))
    {
        for(DWORD i = 0;; i++)
        {
            hr = m_pReader->GetNativeMediaType(
                (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                i,
                &pType
                );

            if(FAILED(hr)) { break; }

            hr = TryMediaType(pType);

            SafeRelease(&pType);

            if(SUCCEEDED(hr))
            {
                // Found an output type.
                break;
            }
        }
    }

    if(FAILED(hr))
    {
        if(pSource)
        {
            pSource->Shutdown();

            // NOTE: The source reader shuts down the media source
            // by default, but we might not have gotten that far.
        }
        CloseDevice();
    }

    SafeRelease(&pSource);
    SafeRelease(&pAttributes);
    SafeRelease(&pType);

    return hr;
}

HRESULT CWebcamAccess::TryMediaType(IMFMediaType *pType)
{
    HRESULT hr = S_OK;

    BOOL bFound = FALSE;
    GUID subtype = { 0 };

    hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);

    if(FAILED(hr))
    {
        return hr;
    }

    // Do we support this type directly?
    if(m_color_converter.IsFormatSupported(subtype))
    {
        bFound = TRUE;
    }
    else
    {
        // Can we decode this media type to one of our supported
        // output formats?

        for(DWORD i = 0;; i++)
        {
            // Get the i'th format.
            m_color_converter.GetFormat(i, &subtype);

            hr = pType->SetGUID(MF_MT_SUBTYPE, subtype);

            if(FAILED(hr)) { break; }

            // Try to set this type on the source reader.
            hr = m_pReader->SetCurrentMediaType(
                (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                NULL,
                pType
                );

            if(SUCCEEDED(hr))
            {
                bFound = TRUE;
                break;
            }
        }
    }

    if(bFound)
    {
        hr = m_color_converter.SetVideoType(pType);
    }

    return hr;
}

void CWebcamAccess::GetImageSizes(unsigned int &width, unsigned int&height)
{
    width = m_color_converter.m_width;
    height = m_color_converter.m_height;
}

HRESULT CWebcamAccess::GetImageData(BYTE *buffer, LONG stride)
{
    if(!m_ready)
        return S_FALSE;

    IMFSample *videoSample = NULL;
    DWORD streamIndex, flags;
    LONGLONG llVideoTimeStamp, llSampleDuration;
    HRESULT hr = S_OK;

    int samplesRead = 0;
    while(!samplesRead)
    {
        hr = m_pReader->ReadSample(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0,                              // Flags.
            &streamIndex,                   // Receives the actual stream index. 
            &flags,                         // Receives status flags.
            &llVideoTimeStamp,              // Receives the time stamp.
            &videoSample                    // Receives the sample or NULL.
            );

        if(SUCCEEDED(hr))
        {
            if(flags & MF_SOURCE_READERF_STREAMTICK)
            {
                //printf("Stream tick.\n");
            }

            if(videoSample)
            {
                samplesRead++;

                hr = videoSample->SetSampleTime(llVideoTimeStamp);
                if(SUCCEEDED(hr))
                {
                    hr = videoSample->GetSampleDuration(&llSampleDuration);
                    if(SUCCEEDED(hr))
                    {
                        IMFMediaBuffer *buf = NULL;
                        hr = videoSample->ConvertToContiguousBuffer(&buf);
                        if(SUCCEEDED(hr))
                        {
                            m_color_converter.ConvertImageToRGB32(buffer, stride, buf);

                            buf->Unlock();
                            buf->Release();
                        }
                    }
                }

                videoSample->Release();
            }
        }
        else
        {
            break;
        }
    }

    return hr;
}
