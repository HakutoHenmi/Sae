#include "SharedMemoryProfiler.h"

namespace Engine {

bool SharedMemoryProfiler::Initialize() {
    // 共有メモリのマッピングオブジェクトを作成
    hMapFile_ = CreateFileMappingA(
        INVALID_HANDLE_VALUE,    // ページングファイルを使用
        NULL,                    // デフォルトのセキュリティ
        PAGE_READWRITE,          // 読み書きアクセス
        0,                       // オブジェクトの最大サイズ（上位32ビット）
        sizeof(SharedEngineData),// オブジェクトの最大サイズ（下位32ビット）
        "NeoEngineSharedMemory"); // 共有メモリの名前

    if (hMapFile_ == NULL) {
        return false;
    }

    // 共有メモリをプロセスのアドレス空間にマッピング
    pBuf_ = (SharedEngineData*)MapViewOfFile(hMapFile_,
        FILE_MAP_ALL_ACCESS, // 読み書き許可
        0,
        0,
        sizeof(SharedEngineData));

    if (pBuf_ == NULL) {
        CloseHandle(hMapFile_);
        hMapFile_ = nullptr;
        return false;
    }

    // データの初期化
    data_ = {};
    *pBuf_ = data_;

    return true;
}

void SharedMemoryProfiler::Shutdown() {
    if (pBuf_) {
        UnmapViewOfFile(pBuf_);
        pBuf_ = nullptr;
    }
    if (hMapFile_) {
        CloseHandle(hMapFile_);
        hMapFile_ = nullptr;
    }
}

void SharedMemoryProfiler::CommitFrame() {
    if (pBuf_) {
        data_.frameNumber++;
        *pBuf_ = data_; // メモリコピーで一気に書き込み
    }
}

} // namespace Engine
