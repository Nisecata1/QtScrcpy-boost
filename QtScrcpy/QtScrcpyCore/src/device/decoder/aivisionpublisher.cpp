#include "aivisionpublisher.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>

#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace {
constexpr uint32_t kMagic = 0x56414353; // "SCAV" (little-endian)
constexpr uint16_t kVersion = 1;
constexpr uint16_t kPixelFormatBgr24 = 1;
constexpr int kMaxRoiSize = 256;
constexpr size_t kMaxPayloadBytes = static_cast<size_t>(kMaxRoiSize) * static_cast<size_t>(kMaxRoiSize) * 3u;
constexpr const char *kMappingName = "Local\\ScrcpyAIVision";
constexpr size_t kSharedMemoryBytes = sizeof(ScrcpyAIVisionHeaderV1) + kMaxPayloadBytes;

std::mutex &sharedPublishMutex()
{
    static std::mutex s_mutex;
    return s_mutex;
}

bool readAIVisionSharedMemoryEnabled()
{
    QString iniPath;
    if (QCoreApplication::instance()) {
        const QString appPath = QCoreApplication::applicationDirPath() + "/config/userdata.ini";
        if (QFileInfo::exists(appPath)) {
            iniPath = appPath;
        }
    }
    if (iniPath.isEmpty()) {
        const QString envConfig = QString::fromLocal8Bit(qgetenv("QTSCRCPY_CONFIG_PATH"));
        if (!envConfig.isEmpty()) {
            const QString candidate = envConfig + "/userdata.ini";
            if (QFileInfo::exists(candidate)) {
                iniPath = candidate;
            }
        }
    }
    if (iniPath.isEmpty()) {
        return true;
    }

    QSettings settings(iniPath, QSettings::IniFormat);
    const QVariant val = settings.value(QStringLiteral("common/AIVisionSharedMemoryEnabled"), true);
    if (val.userType() == QMetaType::Bool) {
        return val.toBool();
    }
    const QString text = val.toString().trimmed().toLower();
    if (text == QLatin1String("false") || text == QLatin1String("0") || text == QLatin1String("off") || text == QLatin1String("no")) {
        return false;
    }
    return true;
}
} // namespace

AIVisionPublisher::AIVisionPublisher()
{
    m_enabled = readAIVisionSharedMemoryEnabled();
    if (m_enabled) {
        m_bgrBuffer.resize(kMaxPayloadBytes);
    }
}

AIVisionPublisher::~AIVisionPublisher()
{
    cleanup();
}

void AIVisionPublisher::cleanup()
{
    std::lock_guard<std::mutex> lock(sharedPublishMutex());
    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }
#if defined(_WIN32)
    if (m_sharedBase) {
        UnmapViewOfFile(m_sharedBase);
        m_sharedBase = nullptr;
    }
    if (m_mappingHandle) {
        CloseHandle(static_cast<HANDLE>(m_mappingHandle));
        m_mappingHandle = nullptr;
    }
#endif
    m_mappingReady = false;
}

bool AIVisionPublisher::ensureMappingReady()
{
    if (m_mappingReady) {
        return true;
    }

#if !defined(_WIN32)
    return false;
#else
    if (!m_mappingHandle) {
        HANDLE mapping = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0,
            static_cast<DWORD>(kSharedMemoryBytes),
            kMappingName);
        if (!mapping) {
            return false;
        }
        m_mappingHandle = mapping;
    }

    if (!m_sharedBase) {
        auto *base = static_cast<unsigned char *>(
            MapViewOfFile(static_cast<HANDLE>(m_mappingHandle), FILE_MAP_ALL_ACCESS, 0, 0, kSharedMemoryBytes));
        if (!base) {
            return false;
        }
        m_sharedBase = base;
    }

    auto *header = reinterpret_cast<ScrcpyAIVisionHeaderV1 *>(m_sharedBase);
    if (header->magic != kMagic || header->version != kVersion || header->header_bytes != sizeof(ScrcpyAIVisionHeaderV1)) {
        std::memset(m_sharedBase, 0, kSharedMemoryBytes);
        header->magic = kMagic;
        header->version = kVersion;
        header->header_bytes = static_cast<uint16_t>(sizeof(ScrcpyAIVisionHeaderV1));
        header->seq = 0;
    } else if ((header->seq & 1u) != 0u) {
        // Recover stale odd seqlock left by an interrupted writer process.
        InterlockedIncrement(reinterpret_cast<volatile LONG *>(&header->seq));
    }

    m_mappingReady = true;
    return true;
#endif
}

bool AIVisionPublisher::prepareBgrRoi(const AVFrame *frame, int &roiX, int &roiY, int &roiW, int &roiH)
{
    if (!frame || frame->width <= 0 || frame->height <= 0) {
        return false;
    }
    if (!frame->data[0] || !frame->data[1] || !frame->data[2]) {
        return false;
    }
    if (frame->format != AV_PIX_FMT_YUV420P && frame->format != AV_PIX_FMT_YUVJ420P) {
        return false;
    }

    const int frameW = frame->width;
    const int frameH = frame->height;

    roiW = std::min(kMaxRoiSize, frameW);
    roiH = std::min(kMaxRoiSize, frameH);
    roiX = (frameW - roiW) / 2;
    roiY = (frameH - roiH) / 2;

    // YUV420 safe alignment
    roiX &= ~1;
    roiY &= ~1;
    roiW &= ~1;
    roiH &= ~1;

    if (roiW < 2 || roiH < 2) {
        return false;
    }

    if (roiX + roiW > frameW) {
        roiX = frameW - roiW;
    }
    if (roiY + roiH > frameH) {
        roiY = frameH - roiH;
    }
    roiX = std::max(0, roiX & ~1);
    roiY = std::max(0, roiY & ~1);

    if (roiX + roiW > frameW || roiY + roiH > frameH) {
        return false;
    }

    const int dstStride = roiW * 3;
    const size_t payloadBytes = static_cast<size_t>(dstStride) * static_cast<size_t>(roiH);
    if (payloadBytes == 0 || payloadBytes > m_bgrBuffer.size()) {
        return false;
    }

    m_swsContext = sws_getCachedContext(
        m_swsContext,
        roiW,
        roiH,
        static_cast<AVPixelFormat>(frame->format),
        roiW,
        roiH,
        AV_PIX_FMT_BGR24,
        SWS_FAST_BILINEAR,
        nullptr,
        nullptr,
        nullptr);
    if (!m_swsContext) {
        return false;
    }

    const uint8_t *srcData[4] = {0};
    int srcLinesize[4] = {0};
    srcData[0] = frame->data[0] + roiY * frame->linesize[0] + roiX;
    srcData[1] = frame->data[1] + (roiY / 2) * frame->linesize[1] + (roiX / 2);
    srcData[2] = frame->data[2] + (roiY / 2) * frame->linesize[2] + (roiX / 2);
    srcLinesize[0] = frame->linesize[0];
    srcLinesize[1] = frame->linesize[1];
    srcLinesize[2] = frame->linesize[2];

    uint8_t *dstData[4] = {0};
    int dstLinesize[4] = {0};
    dstData[0] = m_bgrBuffer.data();
    dstLinesize[0] = dstStride;

    const int scaled = sws_scale(m_swsContext, srcData, srcLinesize, 0, roiH, dstData, dstLinesize);
    if (scaled != roiH) {
        return false;
    }

    return true;
}

void AIVisionPublisher::publishBestEffort(const AVFrame *frame)
{
    if (!m_enabled) {
        return;
    }

    std::lock_guard<std::mutex> lock(sharedPublishMutex());
    if (!ensureMappingReady()) {
        return;
    }

#if defined(_WIN32)
    if (!m_sharedBase) {
        return;
    }

    int roiX = 0;
    int roiY = 0;
    int roiW = 0;
    int roiH = 0;
    if (!prepareBgrRoi(frame, roiX, roiY, roiW, roiH)) {
        return;
    }

    const uint32_t stride = static_cast<uint32_t>(roiW * 3);
    const uint32_t payloadBytes = static_cast<uint32_t>(stride * static_cast<uint32_t>(roiH));
    if (payloadBytes == 0 || payloadBytes > kMaxPayloadBytes) {
        return;
    }

    auto *header = reinterpret_cast<ScrcpyAIVisionHeaderV1 *>(m_sharedBase);
    auto *payload = m_sharedBase + sizeof(ScrcpyAIVisionHeaderV1);

    if ((header->seq & 1u) != 0u) {
        InterlockedIncrement(reinterpret_cast<volatile LONG *>(&header->seq));
    }

    // lock-free writer protocol: odd while writing, even when stable.
    InterlockedIncrement(reinterpret_cast<volatile LONG *>(&header->seq));
    MemoryBarrier();

    header->magic = kMagic;
    header->version = kVersion;
    header->header_bytes = static_cast<uint16_t>(sizeof(ScrcpyAIVisionHeaderV1));
    header->frame_id = ++m_frameId;
    header->roi_x = static_cast<uint32_t>(roiX);
    header->roi_y = static_cast<uint32_t>(roiY);
    header->width = static_cast<uint16_t>(roiW);
    header->height = static_cast<uint16_t>(roiH);
    header->stride = static_cast<uint16_t>(stride);
    header->pixel_format = kPixelFormatBgr24;
    header->timestamp_us = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
    header->payload_bytes = payloadBytes;
    header->reserved = 0;

    std::memcpy(payload, m_bgrBuffer.data(), payloadBytes);

    MemoryBarrier();
    InterlockedIncrement(reinterpret_cast<volatile LONG *>(&header->seq));
#else
    (void)frame;
#endif
}
