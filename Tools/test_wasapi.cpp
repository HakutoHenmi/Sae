#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <iostream>

#pragma comment(lib, "ole32.lib")

int main() {
    CoInitialize(NULL);
    IMMDeviceEnumerator* pEnum = NULL;
    CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
    
    IMMDevice* pDevice = NULL;
    pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    
    if(!pDevice) { std::cout << "No default endpoint\n"; return 1; }
    
    IAudioClient* pAudioClient = NULL;
    pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
    
    if(!pAudioClient) { std::cout << "No audio client\n"; return 1; }
    
    WAVEFORMATEX* pwfx = NULL;
    pAudioClient->GetMixFormat(&pwfx);
    
    HRESULT hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        0, 0, pwfx, NULL);
        
    if (FAILED(hr)) {
        std::cout << "Init Failed: " << std::hex << hr << std::endl;
    } else {
        std::cout << "Init Success! Format: " << pwfx->wFormatTag << " channels: " << pwfx->nChannels << " sampleRate: " << pwfx->nSamplesPerSec << std::endl;
        
        IAudioCaptureClient* pCapture = NULL;
        hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCapture);
        if (FAILED(hr)) {
            std::cout << "GetService Failed: " << std::hex << hr << std::endl;
        } else {
            pAudioClient->Start();
            std::cout << "Started capture." << std::endl;
            for(int i=0; i<5; i++) {
                Sleep(100);
                UINT32 packetLength = 0;
                pCapture->GetNextPacketSize(&packetLength);
                std::cout << "Packet size: " << packetLength << std::endl;
            }
        }
    }
    
    return 0;
}
