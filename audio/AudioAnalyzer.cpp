#include "AudioAnalyzer.h"
#include "AnalysisManager.h"
#include "IBeatDetector.h"

#include <aubio/aubio.h>
#include <QFile>
#include <QFileInfo>
#include <QAudioDecoder>
#include <QAudioFormat>
#include <QUrl>
#include <QEventLoop>
#include <QTimer>
#include <cstring>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

AudioAnalyzer::AudioAnalyzer(QObject* parent)
    : QObject(parent)
{
}

AudioAnalyzer::~AudioAnalyzer() = default;

bool AudioAnalyzer::loadFile(const QString& path, QString* error)
{
    m_data = AudioData{};
    QString suffix = QFileInfo(path).suffix().toLower();

    if (suffix == "wav")
        return loadWav(path, error);
    return loadWithQtDecoder(path, error);
}

// ============================================================================
// WAV loader
// ============================================================================

bool AudioAnalyzer::loadWav(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (error) *error = QStringLiteral("Cannot open file: %1").arg(path);
        return false;
    }

    char header[44];
    if (file.read(header, 44) != 44)
    {
        if (error) *error = QStringLiteral("Invalid WAV file (too small)");
        return false;
    }

    if (std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0)
    {
        if (error) *error = QStringLiteral("Not a valid WAV file");
        return false;
    }

    uint16_t audioFormat = *reinterpret_cast<const uint16_t*>(header + 20);
    uint16_t channels = *reinterpret_cast<const uint16_t*>(header + 22);
    uint32_t sampleRate = *reinterpret_cast<const uint32_t*>(header + 24);
    uint16_t bitsPerSample = *reinterpret_cast<const uint16_t*>(header + 34);

    if (audioFormat != 1)
    {
        if (error) *error = QStringLiteral("Only PCM WAV files are supported");
        return false;
    }

    file.seek(0);
    QByteArray fileData = file.readAll();
    const char* data = fileData.constData();
    int dataSize = static_cast<int>(fileData.size());

    int offset = 12; // Skip RIFF header (4 bytes "RIFF" + 4 bytes size + 4 bytes "WAVE")
    int pcmDataSize = 0;
    const char* pcmData = nullptr;
    while (offset + 8 <= dataSize)
    {
        uint32_t chunkSize = *reinterpret_cast<const uint32_t*>(data + offset + 4);

        if (std::memcmp(data + offset, "data", 4) == 0)
        {
            pcmData = data + offset + 8;
            pcmDataSize = static_cast<int>(chunkSize);
            break;
        }
        offset += 8 + static_cast<int>(chunkSize);
        if (offset % 2 != 0) offset++;
    }

    if (!pcmData)
    {
        if (error) *error = QStringLiteral("No data chunk found in WAV");
        return false;
    }

    int bytesPerSample = bitsPerSample / 8;
    int numFrames = pcmDataSize / (bytesPerSample * channels);

    m_data.sampleRate = static_cast<int>(sampleRate);
    m_data.channels = channels;
    m_data.durationSec = static_cast<double>(numFrames) / sampleRate;
    m_data.samples.resize(numFrames);

    for (int i = 0; i < numFrames; ++i)
    {
        float sum = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
        {
            int byteOffset = (i * channels + ch) * bytesPerSample;
            if (byteOffset + bytesPerSample > pcmDataSize) break;

            float sample = 0.0f;
            if (bitsPerSample == 16)
            {
                int16_t val = *reinterpret_cast<const int16_t*>(pcmData + byteOffset);
                sample = static_cast<float>(val) / 32768.0f;
            }
            else if (bitsPerSample == 8)
            {
                int8_t val = static_cast<int8_t>(pcmData[byteOffset] - 128);
                sample = static_cast<float>(val) / 128.0f;
            }
            else if (bitsPerSample == 24)
            {
                int32_t val = static_cast<int32_t>(static_cast<uint8_t>(pcmData[byteOffset]))
                            | (static_cast<int32_t>(static_cast<uint8_t>(pcmData[byteOffset + 1])) << 8)
                            | (static_cast<int32_t>(static_cast<int8_t>(pcmData[byteOffset + 2])) << 16);
                sample = static_cast<float>(val) / 8388608.0f;
            }
            sum += sample;
        }
        m_data.samples[i] = sum / channels;
    }

    return true;
}

// ============================================================================
// Qt Decoder (mp3, flac, ogg, etc.)
// ============================================================================

bool AudioAnalyzer::loadWithQtDecoder(const QString& path, QString* error)
{
    QAudioDecoder decoder;
    decoder.setSource(QUrl::fromLocalFile(path));

    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Float);
    decoder.setAudioFormat(format);

    QEventLoop loop;
    bool failed = false;
    bool finished = false;
    QByteArray allPcm;
    int actualSampleRate = 44100;

    connect(&decoder, &QAudioDecoder::bufferReady, this, [&]() {
        QAudioBuffer buffer = decoder.read();
        if (!buffer.isValid()) return;

        actualSampleRate = buffer.format().sampleRate();
        const float* data = buffer.constData<float>();
        int numSamples = buffer.sampleCount();
        int channels = buffer.format().channelCount();

        for (int i = 0; i < numSamples; i += channels)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
                sum += data[i + ch];
            float sample = sum / channels;
            const char* ptr = reinterpret_cast<const char*>(&sample);
            allPcm.append(ptr, sizeof(float));
        }
    });

    connect(&decoder, &QAudioDecoder::finished, this, [&]() {
        finished = true;
        loop.quit();
    });

    connect(&decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), this,
            [&](QAudioDecoder::Error err) {
        Q_UNUSED(err);
        if (error) *error = QStringLiteral("Audio decode error: %1").arg(decoder.errorString());
        failed = true;
        loop.quit();
    });

    decoder.start();
    QTimer::singleShot(30000, &loop, &QEventLoop::quit);
    loop.exec();

    if (failed) return false;

    if (!finished || allPcm.isEmpty())
    {
        if (error) *error = QStringLiteral("Failed to decode audio file (timeout or no data)");
        return false;
    }

    int numSamples = allPcm.size() / static_cast<int>(sizeof(float));
    m_data.samples.resize(numSamples);
    std::memcpy(m_data.samples.data(), allPcm.constData(), allPcm.size());
    m_data.sampleRate = actualSampleRate;
    m_data.channels = 1;
    m_data.durationSec = static_cast<double>(numSamples) / actualSampleRate;

    return true;
}

// ============================================================================
// Beat Detection — delegates to AnalysisManager
// ============================================================================

BeatAnalysisResult AudioAnalyzer::detectBeats(const AudioData& data)
{
    if (m_analysisManager)
        return m_analysisManager->analyzeSync(data);

    // Fallback: no analysis manager, return empty
    BeatAnalysisResult r;
    r.method = "none";
    return r;
}

// ============================================================================
// Spectrum
// ============================================================================

SpectrumFrame AudioAnalyzer::computeSpectrum(const AudioData& data, int startSample, int fftSize)
{
    SpectrumFrame frame;
    if (data.samples.isEmpty() || fftSize <= 0)
        return frame;

    int maxStart = static_cast<int>(data.samples.size()) - fftSize;
    startSample = std::max(0, std::min(startSample, std::max(0, maxStart)));

    frame.magnitudes.resize(fftSize / 2);

    aubio_fft_t* fft = new_aubio_fft(static_cast<uint_t>(fftSize));
    if (!fft) return frame;

    fvec_t* input = new_fvec(static_cast<uint_t>(fftSize));
    cvec_t* spectrum = new_cvec(static_cast<uint_t>(fftSize));

    for (int i = 0; i < fftSize && (startSample + i) < data.samples.size(); ++i)
    {
        float window = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i / (fftSize - 1)));
        fvec_set_sample(input, data.samples[startSample + i] * window, static_cast<uint_t>(i));
    }

    aubio_fft_do(fft, input, spectrum);

    for (int i = 0; i < fftSize / 2; ++i)
        frame.magnitudes[i] = spectrum->norm[static_cast<uint_t>(i)];

    del_aubio_fft(fft);
    del_fvec(input);
    del_cvec(spectrum);

    return frame;
}
