#include "Audio.h"

#include "AudioBackend.h"
#include "core/File.h"
#include "core/SettingsIni.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <sstream>

namespace {

constexpr int kMaxVolume = 127;

int ClampMilesVolume(int value)
{
    return (std::max)(0, (std::min)(kMaxVolume, value));
}

float ClampUnit(float value)
{
    return (std::max)(0.0f, (std::min)(1.0f, value));
}

bool CopyBuffer(unsigned char** dst, int* dstSize, const unsigned char* src, int size)
{
    if (!dst || !dstSize || !src || size <= 0) {
        return false;
    }

    unsigned char* bytes = new unsigned char[size];
    std::memcpy(bytes, src, static_cast<size_t>(size));
    *dst = bytes;
    *dstSize = size;
    return true;
}

std::unique_ptr<IAudioBackend> CreateDefaultAudioBackend()
{
#if RO_ENABLE_MILES_AUDIO
    return CreateMilesAudioBackend();
#else
    return CreateMiniaudioAudioBackend();
#endif
}

} // namespace

CAudio::CAudio()
    : m_enabled(LoadSettingsIniInt("Audio", "Enabled", 1) != 0)
    , m_soundVolume(1.0f)
    , m_loopedSoundVolume(1.0f)
    , m_loopedSoundPlaying(false)
    , m_bgmVolume(kMaxVolume)
    , m_bgmPaused(false)
    , m_bgmSuppressed(false)
{
}

CAudio::~CAudio()
{
    Shutdown();
}

CAudio* CAudio::GetInstance()
{
    static CAudio instance;
    return &instance;
}

bool CAudio::Init()
{
    if (!m_enabled) {
        return true;
    }

    if (!m_backend) {
        m_backend = CreateDefaultAudioBackend();
    }
    return m_backend && m_backend->Init();
}

void CAudio::Shutdown()
{
    StopLoopedSound();
    if (!m_backend) {
        return;
    }

    m_backend->Shutdown();
    m_backend.reset();
    m_bgmPath.clear();
}

bool CAudio::PlaySound(const char* fName, float vol)
{
    if (!m_enabled || !fName || !*fName || !g_soundMode || !g_isSoundOn) {
        return false;
    }

    const float clampedVolume = ClampUnit(vol * m_soundVolume);
    if (clampedVolume <= 0.0f || !Init()) {
        return false;
    }

    return m_backend->PlayWave(fName, clampedVolume);
}

bool CAudio::PlayLoopedSound(const char* fName, float vol)
{
    if (!m_enabled) {
        return false;
    }

    const std::string path = NormalizeAudioPath(fName);
    if (path.empty()) {
        StopLoopedSound();
        return false;
    }

    if (path != m_loopedSoundPath) {
        StopLoopedSound();
        m_loopedSoundPath = path;
    }
    m_loopedSoundVolume = ClampUnit(vol);
    return SyncLoopedSound();
}

void CAudio::StopLoopedSound()
{
    if (m_backend) {
        m_backend->StopLoopedWave();
    }
    m_loopedSoundPlaying = false;
    m_loopedSoundPath.clear();
}

bool CAudio::SyncLoopedSound()
{
    if (!m_enabled || m_loopedSoundPath.empty()) {
        return false;
    }

    const float volume = ClampUnit(m_loopedSoundVolume * m_soundVolume);
    if (!g_soundMode || !g_isSoundOn || volume <= 0.0f) {
        if (m_backend) {
            m_backend->StopLoopedWave();
        }
        m_loopedSoundPlaying = false;
        // Keep the requested loop so unmuting can resume it on this map.
        return false;
    }

    if (!Init()) {
        return false;
    }
    if (m_loopedSoundPlaying) {
        m_backend->SetLoopedWaveVolume(volume);
        return true;
    }

    m_loopedSoundPlaying = m_backend->PlayLoopedWave(m_loopedSoundPath.c_str(), volume);
    return m_loopedSoundPlaying;
}

bool CAudio::PlaySound3D(const char* fName, const vector3d& soundPos, const vector3d& listenerPos,
    int volumeMaxDist, int volumeMinDist, float vol)
{
    const float dx = soundPos.x - listenerPos.x;
    const float dy = soundPos.y - listenerPos.y;
    const float dz = soundPos.z - listenerPos.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float maxDist = static_cast<float>((std::max)(volumeMaxDist, 1));
    const float minDist = static_cast<float>((std::max)(volumeMinDist, 1));
    if (distance >= maxDist) {
        return false;
    }

    float distanceFactor = 1.0f;
    if (distance > minDist) {
        distanceFactor = minDist / distance;
    }
    return PlaySound(fName, ClampUnit(vol * distanceFactor));
}

void CAudio::PlayBGM(const char* fName)
{
    if (!m_enabled || m_bgmSuppressed || !fName || !*fName || !Init()) {
        return;
    }

    const std::string normalizedPath = NormalizeAudioPath(fName);
    if (normalizedPath.empty()) {
        return;
    }

    if (m_backend->PlayBGM(normalizedPath.c_str(), m_bgmVolume, m_bgmPaused)) {
        m_bgmPath = m_backend->GetCurrentBgmPath();
    }
}

void CAudio::StopBGM()
{
    if (m_backend) {
        m_backend->StopBGM();
    }
    m_bgmPath.clear();
}

void CAudio::SetBgmSuppressed(bool suppressed)
{
    if (m_bgmSuppressed == suppressed) {
        return;
    }
    m_bgmSuppressed = suppressed;
    if (suppressed) {
        StopBGM();
    }
}

void CAudio::StopAllSounds()
{
    if (m_backend) {
        m_backend->StopAllSounds();
    }
    m_loopedSoundPlaying = false;
    m_loopedSoundPath.clear();
}

void CAudio::SetVolume(float vol)
{
    m_soundVolume = ClampUnit(vol);
    SyncLoopedSound();
}

float CAudio::GetVolume() const
{
    return m_soundVolume;
}

void CAudio::SetBgmVolume(int vol)
{
    m_bgmVolume = ClampMilesVolume(vol);
    if (m_backend) {
        m_backend->SetBgmVolume(m_bgmVolume);
    }
}

int CAudio::GetBgmVolume() const
{
    return m_bgmVolume;
}

void CAudio::SetBgmPaused(bool paused)
{
    m_bgmPaused = paused;
    if (m_backend) {
        m_backend->SetBgmPaused(paused);
    }
}

bool CAudio::IsBgmPaused() const
{
    return m_bgmPaused;
}

const std::string& CAudio::GetCurrentBgmPath() const
{
    return m_bgmPath;
}

std::string CAudio::CanonicalizeMapName(const char* rswName)
{
    std::string name = rswName ? rswName : "";
    for (char& ch : name) {
        if (ch == '/') {
            ch = '\\';
        }
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return name;
}

std::string CAudio::NormalizeAudioPath(const char* path)
{
    std::string normalized = path ? path : "";
    for (char& ch : normalized) {
        if (ch == '/') {
            ch = '\\';
        }
    }
    return normalized;
}

bool CAudio::EnsureMapBgmTableLoaded()
{
    if (!m_mapBgmTable.empty()) {
        return true;
    }

    int size = 0;
    unsigned char* bytes = g_fileMgr.GetData("data\\mp3nametable.txt", &size);
    if (!bytes || size <= 0) {
        delete[] bytes;
        return false;
    }

    std::string content(reinterpret_cast<const char*>(bytes), static_cast<size_t>(size));
    delete[] bytes;

    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        const size_t firstHash = line.find('#');
        if (firstHash == std::string::npos) {
            continue;
        }
        const size_t secondHash = line.find('#', firstHash + 1u);
        if (secondHash == std::string::npos || secondHash <= firstHash + 1u) {
            continue;
        }

        MapBgmEntry entry{};
        entry.rswName = CanonicalizeMapName(line.substr(0, firstHash).c_str());
        entry.bgmPath = NormalizeAudioPath(line.substr(firstHash + 1u, secondHash - firstHash - 1u).c_str());
        if (!entry.rswName.empty() && !entry.bgmPath.empty()) {
            m_mapBgmTable.push_back(entry);
        }
    }

    return !m_mapBgmTable.empty();
}

std::string CAudio::ResolveMapBgmPath(const char* rswName)
{
    const std::string canonical = CanonicalizeMapName(rswName);
    if (canonical.empty()) {
        return "bgm\\01.mp3";
    }

    EnsureMapBgmTableLoaded();
    for (const MapBgmEntry& entry : m_mapBgmTable) {
        if (entry.rswName == canonical) {
            return entry.bgmPath;
        }
    }
    return "bgm\\01.mp3";
}

CWave::CWave() : m_fileImage(nullptr), m_isDecompress(0), m_fileSize(0) {}
CWave::~CWave() { Reset(); }

bool CWave::Load(const char* fName)
{
    Reset();

    int size = 0;
    unsigned char* bytes = g_fileMgr.GetData(fName, &size);
    if (!bytes || size <= 0) {
        delete[] bytes;
        return false;
    }

    if (!CopyBuffer(&m_fileImage, &m_fileSize, bytes, size)) {
        delete[] bytes;
        return false;
    }

    delete[] bytes;
    return true;
}

CRes* CWave::Clone() { return new CWave(); }
void CWave::Reset()
{
    delete[] m_fileImage;
    m_fileImage = nullptr;
    m_fileSize = 0;
}
