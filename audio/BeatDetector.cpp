// Include torch FIRST to avoid Qt macro conflicts
#include <torch/script.h>

#include "BeatDetector.h"

#include <cmath>
#include <QFile>
#include <QDir>
#include <algorithm>
#include <numeric>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

BeatDetector::BeatDetector()
{
}

BeatDetector::~BeatDetector()
{
    auto* module = static_cast<torch::jit::script::Module*>(m_module);
    delete module;
    m_module = nullptr;
}

bool BeatDetector::loadModel(const QString& modelPath)
{
    try
    {
        auto* module = new torch::jit::script::Module();
        std::string path = modelPath.toStdString();
        *module = torch::jit::load(path);
        module->eval();
        delete static_cast<torch::jit::script::Module*>(m_module);
        m_module = module;
        m_modelLoaded = true;
        return true;
    }
    catch (const c10::Error& e)
    {
        qWarning("BeatDetector: Failed to load model: %s", e.what());
        m_modelLoaded = false;
        return false;
    }
}

BeatInfo BeatDetector::detect(const AudioData& data)
{
    BeatInfo info;
    if (!m_modelLoaded || data.samples.isEmpty())
        return info;

    // Resample to 22050 Hz if needed (BeatNet expects 22050)
    QVector<float> audio = data.samples;
    int sr = data.sampleRate;
    if (sr != m_sampleRate)
    {
        double ratio = static_cast<double>(m_sampleRate) / sr;
        int newLen = static_cast<int>(audio.size() * ratio);
        audio.resize(newLen);
        for (int i = 0; i < newLen; ++i)
        {
            double srcPos = i / ratio;
            int idx = static_cast<int>(srcPos);
            double frac = srcPos - idx;
            if (idx + 1 < data.samples.size())
                audio[i] = static_cast<float>(data.samples[idx] * (1.0 - frac) + data.samples[idx + 1] * frac);
            else if (idx < data.samples.size())
                audio[i] = data.samples[idx];
            else
                audio[i] = 0.0f;
        }
        sr = m_sampleRate;
    }

    // Compute log-spectrogram features (272-dim per frame)
    std::vector<std::vector<float>> features = computeLogSpectrogram(audio.constData(), audio.size(), sr);

    if (features.empty())
        return info;

    int numFrames = static_cast<int>(features.size());
    int featDim = static_cast<int>(features[0].size());

    // Create input tensor: [1, 1, numFrames, 272]
    std::vector<float> flat;
    flat.reserve(numFrames * featDim);
    for (const auto& f : features)
        flat.insert(flat.end(), f.begin(), f.end());

    auto options = torch::TensorOptions().dtype(torch::kFloat32);
    torch::Tensor input = torch::from_blob(
        flat.data(), {1, 1, numFrames, featDim}, options).clone();

    // Run inference
    std::vector<torch::jit::IValue> inputs;
    inputs.push_back(input);

    torch::Tensor output;
    try
    {
        auto* module = static_cast<torch::jit::script::Module*>(m_module);
        output = module->forward(inputs).toTensor();
    }
    catch (const c10::Error& e)
    {
        qWarning("BeatDetector: Inference failed: %s", e.what());
        return info;
    }

    if (output.dim() != 3)
    {
        qWarning("BeatDetector: Unexpected output rank %lld", static_cast<long long>(output.dim()));
        return info;
    }

    int classDim = -1;
    if (output.size(1) == 3)
        classDim = 1;  // [1, 3, frames]
    else if (output.size(2) == 3)
        classDim = 2;  // [1, frames, 3]
    else
    {
        qWarning("BeatDetector: Unexpected output shape [%lld, %lld, %lld]",
                 static_cast<long long>(output.size(0)),
                 static_cast<long long>(output.size(1)),
                 static_cast<long long>(output.size(2)));
        return info;
    }

    torch::Tensor probs = torch::softmax(output, classDim);
    // BeatNet BDA emits classes as: beat, downbeat, non-beat.
    torch::Tensor beatProbs = probs.select(classDim, 0).contiguous();
    torch::Tensor downbeatProbs = probs.select(classDim, 1).contiguous();

    std::vector<float> beatActivations(beatProbs.data_ptr<float>(),
                                       beatProbs.data_ptr<float>() + beatProbs.numel());
    std::vector<float> downbeatActivations(downbeatProbs.data_ptr<float>(),
                                           downbeatProbs.data_ptr<float>() + downbeatProbs.numel());

    const size_t activationCount = std::min(beatActivations.size(), downbeatActivations.size());
    std::vector<float> activations(activationCount);
    for (size_t i = 0; i < activationCount; ++i)
        activations[i] = std::clamp(beatActivations[i] + downbeatActivations[i], 0.0f, 1.0f);

    // Percentile normalization: align distribution shape with Python runtime
    // C++ TorchScript produces ~1.5x higher baseline than Python.
    // Normalize p10→0, p90→1 to match Python's activation distribution.
    if (activations.size() > 10)
    {
        std::vector<float> sorted = activations;
        std::sort(sorted.begin(), sorted.end());
        const float p10 = sorted[static_cast<size_t>(sorted.size() * 0.10)];
        const float p90 = sorted[static_cast<size_t>(sorted.size() * 0.90)];
        const float range = p90 - p10;
        if (range > 1e-6f)
        {
            for (auto& a : activations)
                a = std::clamp((a - p10) / range, 0.0f, 1.0f);
        }
    }

    if (activations.empty())
        return info;

    // Convert frame indices to time
    double durationSec = static_cast<double>(audio.size()) / sr;

    // Track beats from activations
    BeatInfo result = trackBeats(activations, downbeatActivations, sr, m_hopSize, durationSec);
    alignBeatsToTransients(result, audio, sr);

    // Store activation curve for visualization
    result.activation = QVector<float>(activations.begin(), activations.end());
    result.downbeatActivation = QVector<float>(downbeatActivations.begin(), downbeatActivations.end());
    result.activationSampleRate = static_cast<double>(sr) / m_hopSize;

    return result;
}

std::vector<std::vector<float>> BeatDetector::computeLogSpectrogram(
    const float* samples, int numSamples, int sampleRate)
{
    int halfFFT = m_fftSize / 2 + 1;
    const int bandCount = m_featureDim / 2;
    if (m_filterbank.size() != bandCount || (!m_filterbank.isEmpty() && m_filterbank[0].size() != halfFFT))
        buildFilterbank(m_fftSize, sampleRate, bandCount);

    std::vector<float> window = hanningWindow(m_fftSize);

    float windowSum = 0.0f;
    for (float w : window) windowSum += w;
    if (windowSum < 1e-8f) windowSum = 1.0f;

    std::vector<std::vector<float>> logBands;

    for (int start = 0; start + m_fftSize <= numSamples; start += m_hopSize)
    {
        std::vector<float> real(m_fftSize, 0.0f);
        std::vector<float> imag(m_fftSize, 0.0f);

        for (int i = 0; i < m_fftSize; ++i)
            real[i] = samples[start + i] * window[i];

        fft(real, imag);

        std::vector<float> magnitude(halfFFT, 0.0f);
        for (int i = 0; i < halfFFT; ++i)
            magnitude[i] = std::sqrt(real[i] * real[i] + imag[i] * imag[i]) / windowSum;

        std::vector<float> frame(bandCount, 0.0f);
        for (int band = 0; band < bandCount; ++band)
        {
            float energy = 0.0f;
            for (int bin = 0; bin < halfFFT; ++bin)
                energy += m_filterbank[band][bin] * magnitude[bin];
            frame[band] = std::log(1.0f + energy);
        }

        logBands.push_back(std::move(frame));
    }

    std::vector<std::vector<float>> result;
    result.reserve(logBands.size());
    std::vector<float> previous(bandCount, 0.0f);
    for (const auto& bands : logBands)
    {
        std::vector<float> frame(m_featureDim, 0.0f);
        for (int band = 0; band < bandCount; ++band)
        {
            frame[band] = bands[band];
            frame[bandCount + band] = std::max(0.0f, (bands[band] - previous[band]) * 0.5f);
        }

        result.push_back(std::move(frame));
        previous = bands;
    }

    return result;
}

void BeatDetector::buildFilterbank(int fftSize, int sampleRate, int numBands)
{
    int halfFFT = fftSize / 2 + 1;
    float freqRes = static_cast<float>(sampleRate) / fftSize;

    float fmin = 30.0f;
    float fmax = std::min(17000.0f, static_cast<float>(sampleRate) / 2.0f);

    m_filterbank.resize(numBands);
    for (int b = 0; b < numBands; ++b)
    {
        m_filterbank[b].resize(halfFFT, 0.0f);

        const float low = fmin * std::pow(fmax / fmin, static_cast<float>(b) / numBands);
        const float center = fmin * std::pow(fmax / fmin, static_cast<float>(b + 1) / numBands);
        const float high = fmin * std::pow(fmax / fmin, static_cast<float>(b + 2) / numBands);

        int lowBin = std::max(0, static_cast<int>(std::floor(low / freqRes)));
        int centerBin = std::clamp(static_cast<int>(std::round(center / freqRes)), 0, halfFFT - 1);
        int highBin = std::clamp(static_cast<int>(std::ceil(high / freqRes)), 0, halfFFT - 1);

        for (int k = lowBin; k < centerBin; ++k)
        {
            if (centerBin > lowBin)
                m_filterbank[b][k] = static_cast<float>(k - lowBin) / (centerBin - lowBin);
        }

        for (int k = centerBin; k <= highBin; ++k)
        {
            if (highBin > centerBin)
                m_filterbank[b][k] = static_cast<float>(highBin - k) / (highBin - centerBin);
        }

        float sum = 0.0f;
        for (float value : m_filterbank[b])
            sum += value;
        if (sum > 1e-8f)
        {
            for (float& value : m_filterbank[b])
                value /= sum;
        }
    }
}

BeatInfo BeatDetector::trackBeats(
    const std::vector<float>& activations,
    const std::vector<float>& downbeatActivations,
    double sampleRate, int hopSize, double durationSec)
{
    BeatInfo info;
    if (activations.empty())
        return info;

    const double fps = sampleRate / hopSize;
    const int frameCount = static_cast<int>(activations.size());

    const auto [minIt, maxIt] = std::minmax_element(activations.begin(), activations.end());
    const float minAct = *minIt;
    const float maxAct = *maxIt;
    const float range = maxAct - minAct;
    if (range < 1e-5f)
        return info;

    // BeatNet outputs can be biased high for some material. Normalize first and
    // then score local contrast, so a saturated-but-wavy curve still yields peaks.
    std::vector<float> normalized(frameCount, 0.0f);
    for (int i = 0; i < frameCount; ++i)
        normalized[i] = std::clamp((activations[i] - minAct) / range, 0.0f, 1.0f);

    std::vector<float> smoothed(frameCount, 0.0f);
    for (int i = 0; i < frameCount; ++i)
    {
        const int from = std::max(0, i - 1);
        const int to = std::min(frameCount - 1, i + 1);
        float sum = 0.0f;
        for (int j = from; j <= to; ++j)
            sum += normalized[j];
        smoothed[i] = sum / static_cast<float>(to - from + 1);
    }

    const int localRadius = std::max(2, static_cast<int>(std::round(0.30 * fps)));
    std::vector<float> scores(frameCount, 0.0f);
    for (int i = 0; i < frameCount; ++i)
    {
        const int from = std::max(0, i - localRadius);
        const int to = std::min(frameCount - 1, i + localRadius);
        float localSum = 0.0f;
        float localMin = std::numeric_limits<float>::max();
        for (int j = from; j <= to; ++j)
        {
            localSum += smoothed[j];
            localMin = std::min(localMin, smoothed[j]);
        }

        const float localMean = localSum / static_cast<float>(to - from + 1);
        const float contrast = smoothed[i] - localMean;
        const float prominence = smoothed[i] - localMin;
        scores[i] = std::max(0.0f, contrast * 0.65f + prominence * 0.35f);
    }

    std::vector<float> sortedScores = scores;
    std::sort(sortedScores.begin(), sortedScores.end());
    const float percentileScore = sortedScores[static_cast<size_t>(sortedScores.size() * 0.82)];
    const float threshold = std::max(0.035f, percentileScore);

    std::vector<int> candidateFrames;
    const int candidateMinGap = std::max(1, static_cast<int>(std::round(0.08 * fps)));
    const int peakRadius = std::max(1, static_cast<int>(std::round(0.04 * fps)));
    const float candidateThreshold = std::max(0.003f, threshold * 0.25f);

    for (int i = peakRadius; i < frameCount - peakRadius; ++i)
    {
        bool isLocalMax = true;
        for (int j = i - peakRadius; j <= i + peakRadius; ++j)
        {
            if (j != i && smoothed[j] > smoothed[i])
            {
                isLocalMax = false;
                break;
            }
        }
        if (!isLocalMax || scores[i] < candidateThreshold || smoothed[i] < 0.03f)
            continue;

        if (candidateFrames.empty() || (i - candidateFrames.back()) >= candidateMinGap)
        {
            candidateFrames.push_back(i);
        }
        else if (scores[i] > scores[candidateFrames.back()])
        {
            candidateFrames.back() = i;
        }
    }

    for (int frame : candidateFrames)
    {
        info.candidatePeakTimes.append(static_cast<double>(frame) / fps);
        info.candidatePeakScores.append(std::clamp(smoothed[frame], 0.0f, 1.0f));
    }

    if (candidateFrames.size() < 2)
    {
        qWarning("BeatDetector: too few BeatNet candidate peaks (%d)",
                 static_cast<int>(candidateFrames.size()));
        return info;
    }

    const int minLag = std::max(1, static_cast<int>(std::round(0.30 * fps)));
    const int maxLag = std::min(frameCount / 2, static_cast<int>(std::round(1.20 * fps)));
    int bestLag = 0;
    double bestLagScore = 0.0;

    std::vector<double> intervalHistogram(maxLag + 1, 0.0);
    for (int i = 0; i < static_cast<int>(candidateFrames.size()); ++i)
    {
        for (int j = i + 1; j < static_cast<int>(candidateFrames.size()); ++j)
        {
            const int gap = candidateFrames[j] - candidateFrames[i];
            if (gap < minLag)
                continue;
            if (gap > maxLag)
                break;

            const double weight = std::max(0.001f, smoothed[candidateFrames[i]]) *
                                  std::max(0.001f, smoothed[candidateFrames[j]]);
            intervalHistogram[gap] += weight;
        }
    }

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double score = 0.0;
        for (int nearLag = std::max(minLag, lag - 2); nearLag <= std::min(maxLag, lag + 2); ++nearLag)
            score += intervalHistogram[nearLag];

        const double bpm = 60.0 * fps / lag;
        if (bpm < 65.0 || bpm > 190.0)
            score *= 0.80;

        if (score > bestLagScore)
        {
            bestLagScore = score;
            bestLag = lag;
        }
    }

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double sum = 0.0;
        int count = 0;
        for (int i = 0; i + lag < frameCount; ++i)
        {
            sum += smoothed[i] * smoothed[i + lag];
            ++count;
        }

        if (count == 0)
            continue;

        const double lagSec = static_cast<double>(lag) / fps;
        const double bpm = 60.0 / lagSec;
        const double tempoPrior = bpm >= 70.0 && bpm <= 180.0 ? 1.0 : 0.82;
        const double score = (sum / count) * tempoPrior;
        if (bestLag == 0 && score > bestLagScore)
        {
            bestLagScore = score;
            bestLag = lag;
        }
    }

    if (bestLag <= 0 || bestLagScore < 0.0005)
    {
        qWarning("BeatDetector: no reliable tempo from BeatNet activation (min=%.4f max=%.4f candidates=%d)",
                 minAct, maxAct, static_cast<int>(candidateFrames.size()));
        return info;
    }

    const int searchRadius = std::max(2, static_cast<int>(std::round(bestLag * 0.18)));
    int bestPhase = 0;
    double bestPhaseScore = -1.0;

    for (int phase = 0; phase < bestLag; ++phase)
    {
        double phaseScore = 0.0;
        int beatCount = 0;
        for (int grid = phase; grid < frameCount; grid += bestLag)
        {
            const int from = std::max(0, grid - searchRadius);
            const int to = std::min(frameCount - 1, grid + searchRadius);
            float localMax = 0.0f;
            for (int j = from; j <= to; ++j)
                localMax = std::max(localMax, smoothed[j]);
            phaseScore += localMax;
            ++beatCount;
        }

        if (beatCount > 0)
            phaseScore /= beatCount;

        if (phaseScore > bestPhaseScore)
        {
            bestPhaseScore = phaseScore;
            bestPhase = phase;
        }
    }

    if (bestPhaseScore < 0.08)
    {
        qWarning("BeatDetector: weak phase confidence from BeatNet activation (phaseScore=%.4f candidates=%d)",
                 bestPhaseScore, static_cast<int>(candidateFrames.size()));
        return info;
    }

    const int localAdjustRadius = std::max(2, static_cast<int>(std::round(bestLag * 0.12)));
    int previousFrame = -bestLag;
    double expectedLag = bestLag;
    std::vector<int> decodedFrames;
    std::vector<float> decodedConfidences;
    for (int grid = bestPhase; grid < frameCount; grid += bestLag)
    {
        const int from = std::max(0, grid - localAdjustRadius);
        const int to = std::min(frameCount - 1, grid + localAdjustRadius);
        int localFrame = grid;
        float localValue = smoothed[grid];
        for (int j = from; j <= to; ++j)
        {
            if (smoothed[j] > localValue)
            {
                localValue = smoothed[j];
                localFrame = j;
            }
        }

        int predictedFrame = previousFrame >= 0
            ? previousFrame + static_cast<int>(std::round(expectedLag))
            : grid;
        predictedFrame = std::clamp(predictedFrame, 0, frameCount - 1);

        const int localInterval = localFrame - previousFrame;
        const bool tempoConsistent = previousFrame < 0 ||
            std::abs(localInterval - expectedLag) <= std::max(2.0, expectedLag * 0.18);
        const bool strongLocal = localValue >= 0.12f &&
            std::abs(localFrame - predictedFrame) <= std::max(2.0, expectedLag * 0.14);

        int frame = (strongLocal && tempoConsistent) ? localFrame : predictedFrame;
        if (frame - previousFrame < expectedLag * 0.55)
            frame = previousFrame + static_cast<int>(std::round(expectedLag));
        if (frame >= frameCount)
            break;

        double timeSec = static_cast<double>(frame) / fps;
        if (timeSec >= 0.0 && timeSec <= durationSec)
        {
            decodedFrames.push_back(frame);
            decodedConfidences.push_back(std::clamp(localValue, 0.15f, 1.0f));
        }

        if (previousFrame >= 0)
            expectedLag = expectedLag * 0.90 + (frame - previousFrame) * 0.10;
        previousFrame = frame;
    }

    int downbeatPhase = -1;
    if (decodedFrames.size() >= 4 && !downbeatActivations.empty())
    {
        double phaseScores[4] = {0.0, 0.0, 0.0, 0.0};
        int phaseCounts[4] = {0, 0, 0, 0};
        for (int i = 0; i < static_cast<int>(decodedFrames.size()); ++i)
        {
            const int frame = decodedFrames[i];
            if (frame >= 0 && frame < static_cast<int>(downbeatActivations.size()))
            {
                phaseScores[i % 4] += downbeatActivations[frame];
                phaseCounts[i % 4]++;
            }
        }

        double bestScore = 0.0;
        for (int phase = 0; phase < 4; ++phase)
        {
            const double score = phaseCounts[phase] > 0 ? phaseScores[phase] / phaseCounts[phase] : 0.0;
            if (score > bestScore)
            {
                bestScore = score;
                downbeatPhase = phase;
            }
        }

        if (bestScore < 0.03)
            downbeatPhase = -1;
    }

    for (int i = 0; i < static_cast<int>(decodedFrames.size()); ++i)
    {
        const int frame = decodedFrames[i];
        info.beatTimes.append(static_cast<double>(frame) / fps);
        info.beatConfidences.append(decodedConfidences[i]);

        bool isDownbeat = false;
        if (downbeatPhase >= 0)
            isDownbeat = (i % 4) == downbeatPhase;
        else if (frame < static_cast<int>(downbeatActivations.size()))
            isDownbeat = downbeatActivations[frame] >= activations[frame] * 0.55f;
        info.beatIsDownbeat.append(isDownbeat);
    }

    info.bpm = 60.0 * fps / bestLag;

    // === Tempo diagnostics ===
    info.selectedTempoSec = static_cast<double>(bestLag) / fps;
    info.selectedBpm = info.bpm;
    info.tempoScore = bestLagScore;
    info.candidateCount = static_cast<int>(candidateFrames.size());

    // Copy interval histogram
    info.histogramMinSec = static_cast<double>(minLag) / fps;
    info.histogramMaxSec = static_cast<double>(maxLag) / fps;
    info.intervalHistogram.clear();
    info.intervalHistogram.reserve(maxLag - minLag + 1);
    for (int lag = minLag; lag <= maxLag; ++lag)
        info.intervalHistogram.append(intervalHistogram[lag]);

    // Median candidate interval
    if (candidateFrames.size() >= 2)
    {
        QVector<double> candIbis;
        for (int i = 1; i < static_cast<int>(candidateFrames.size()); ++i)
        {
            double ibi = static_cast<double>(candidateFrames[i] - candidateFrames[i - 1]) / fps;
            if (ibi > 0.1 && ibi < 3.0)
                candIbis.append(ibi);
        }
        if (!candIbis.isEmpty())
        {
            std::sort(candIbis.begin(), candIbis.end());
            info.medianCandidateInterval = candIbis[candIbis.size() / 2];
        }
    }

    if (info.beatTimes.size() < 2 && durationSec > 2.0)
    {
        qWarning("BeatDetector: tempo decoder produced too few beats (%d) for %.2fs audio",
                 static_cast<int>(info.beatTimes.size()), durationSec);
        info.beatTimes.clear();
        info.beatIsDownbeat.clear();
        info.beatConfidences.clear();
        info.bpm = 0.0;
    }

    return info;
}

void BeatDetector::alignBeatsToTransients(BeatInfo& info, const QVector<float>& audio, int sampleRate)
{
    if (info.beatTimes.size() < 8 || audio.isEmpty() || sampleRate <= 0)
        return;

    const int frameSize = std::max(128, static_cast<int>(std::round(0.023 * sampleRate)));
    const int hopSize = std::max(64, static_cast<int>(std::round(0.010 * sampleRate)));
    const int sampleCount = static_cast<int>(audio.size());
    const int frameCount = std::max(0, (sampleCount - frameSize) / hopSize);
    if (frameCount < 4)
        return;

    std::vector<float> attackEnergy(frameCount, 0.0f);
    for (int frame = 0; frame < frameCount; ++frame)
    {
        const int start = frame * hopSize;
        double sum = 0.0;
        float previous = start > 0 ? audio[start - 1] : audio[start];
        for (int i = 0; i < frameSize; ++i)
        {
            const float sample = audio[start + i];
            const float high = sample - previous;
            sum += high * high;
            previous = sample;
        }
        attackEnergy[frame] = static_cast<float>(std::sqrt(sum / frameSize));
    }

    std::vector<float> onset(frameCount, 0.0f);
    for (int frame = 1; frame < frameCount; ++frame)
        onset[frame] = std::max(0.0f, attackEnergy[frame] - attackEnergy[frame - 1] * 0.65f);

    std::vector<float> sortedOnset = onset;
    std::sort(sortedOnset.begin(), sortedOnset.end());
    const float strongOnset = sortedOnset[static_cast<size_t>(sortedOnset.size() * 0.75)];
    if (strongOnset <= 1e-5f)
    {
        constexpr double kPerceptualPreShiftSec = -0.035;
        for (double& beatTime : info.beatTimes)
            beatTime = std::max(0.0, beatTime + kPerceptualPreShiftSec);
        info.perceptualShiftSec = kPerceptualPreShiftSec;
        return;
    }

    QVector<double> offsets;
    offsets.reserve(info.beatTimes.size());
    for (double beatTime : info.beatTimes)
    {
        const double fromSec = beatTime - 0.16;
        const double toSec = beatTime + 0.04;
        const int from = std::max(1, static_cast<int>(std::floor(fromSec * sampleRate / hopSize)));
        const int to = std::min(frameCount - 1, static_cast<int>(std::ceil(toSec * sampleRate / hopSize)));
        if (to <= from)
            continue;

        int bestFrame = from;
        float bestValue = onset[from];
        for (int frame = from + 1; frame <= to; ++frame)
        {
            if (onset[frame] > bestValue)
            {
                bestValue = onset[frame];
                bestFrame = frame;
            }
        }

        if (bestValue < strongOnset)
            continue;

        const double onsetTime = static_cast<double>(bestFrame * hopSize) / sampleRate;
        const double offset = onsetTime - beatTime;
        if (offset >= -0.16 && offset <= 0.04)
            offsets.append(offset);
    }

    constexpr double kPerceptualPreShiftSec = -0.035;
    const int minMatches = std::max(6, static_cast<int>(info.beatTimes.size() / 5));
    if (offsets.size() < minMatches)
    {
        for (double& beatTime : info.beatTimes)
            beatTime = std::max(0.0, beatTime + kPerceptualPreShiftSec);
        info.perceptualShiftSec = kPerceptualPreShiftSec;
        return;
    }

    std::sort(offsets.begin(), offsets.end());
    double medianOffset = offsets[offsets.size() / 2];
    medianOffset = std::clamp(medianOffset, -0.12, 0.04);
    const double totalOffset = medianOffset + kPerceptualPreShiftSec;

    for (double& beatTime : info.beatTimes)
        beatTime = std::max(0.0, beatTime + totalOffset);

    info.attackPhaseOffsetSec = medianOffset;
    info.perceptualShiftSec = kPerceptualPreShiftSec;
    info.attackPhaseMatches = offsets.size();

    qInfo("BeatDetector: attack phase %.1f ms + perceptual shift %.1f ms (%d matched beats)",
          medianOffset * 1000.0, kPerceptualPreShiftSec * 1000.0, offsets.size());
}

std::vector<float> BeatDetector::hanningWindow(int size)
{
    std::vector<float> window(size);
    for (int i = 0; i < size; ++i)
    {
        window[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i / (size - 1)));
    }
    return window;
}

void BeatDetector::fft(std::vector<float>& real, std::vector<float>& imag)
{
    int n = static_cast<int>(real.size());
    if (n <= 1) return;

    int logN = 0;
    for (int temp = n; temp > 1; temp >>= 1) logN++;

    for (int i = 0; i < n; ++i)
    {
        int j = 0;
        for (int bit = 0; bit < logN; ++bit)
        {
            if (i & (1 << bit))
                j |= (1 << (logN - 1 - bit));
        }
        if (i < j)
        {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
    }

    for (int len = 2; len <= n; len <<= 1)
    {
        float angle = -2.0f * static_cast<float>(M_PI) / len;
        float wReal = std::cos(angle);
        float wImag = std::sin(angle);

        for (int i = 0; i < n; i += len)
        {
            float curReal = 1.0f;
            float curImag = 0.0f;

            for (int j = 0; j < len / 2; ++j)
            {
                int uIdx = i + j;
                int tIdx = i + j + len / 2;

                float tReal = curReal * real[tIdx] - curImag * imag[tIdx];
                float tImag = curReal * imag[tIdx] + curImag * real[tIdx];

                real[tIdx] = real[uIdx] - tReal;
                imag[tIdx] = imag[uIdx] - tImag;
                real[uIdx] += tReal;
                imag[uIdx] += tImag;

                float newCurReal = curReal * wReal - curImag * wImag;
                curImag = curReal * wImag + curImag * wReal;
                curReal = newCurReal;
            }
        }
    }
}

// ============================================================================
// Feature dump for validation
// ============================================================================

std::vector<std::vector<float>> BeatDetector::getFeatures(const AudioData& data)
{
    if (data.samples.isEmpty()) return {};

    QVector<float> audio = data.samples;
    int sr = data.sampleRate;
    if (sr != m_sampleRate)
    {
        double ratio = static_cast<double>(m_sampleRate) / sr;
        int newLen = static_cast<int>(audio.size() * ratio);
        audio.resize(newLen);
        for (int i = 0; i < newLen; ++i)
        {
            double srcPos = i / ratio;
            int idx = static_cast<int>(srcPos);
            double frac = srcPos - idx;
            if (idx + 1 < data.samples.size())
                audio[i] = static_cast<float>(data.samples[idx] * (1.0 - frac) + data.samples[idx + 1] * frac);
            else if (idx < data.samples.size())
                audio[i] = data.samples[idx];
            else
                audio[i] = 0.0f;
        }
        sr = m_sampleRate;
    }

    return computeLogSpectrogram(audio.constData(), audio.size(), sr);
}

bool BeatDetector::dumpFeatures(const AudioData& data, const QString& outputPath)
{
    auto features = getFeatures(data);
    if (features.empty()) return false;

    int numFrames = static_cast<int>(features.size());
    int featDim = static_cast<int>(features[0].size());

    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly))
    {
        qWarning("BeatDetector: Cannot write to %s", qPrintable(outputPath));
        return false;
    }

    // Header: numFrames (int32), featDim (int32)
    int32_t header[2] = { numFrames, featDim };
    file.write(reinterpret_cast<const char*>(header), sizeof(header));

    // Data: row-major float32
    for (const auto& frame : features)
    {
        file.write(reinterpret_cast<const char*>(frame.data()), featDim * sizeof(float));
    }

    file.close();
    qInfo("BeatDetector: Dumped features [%d x %d] to %s", numFrames, featDim, qPrintable(outputPath));
    return true;
}

// ============================================================================
// Stage-by-stage dump for validation
// ============================================================================

namespace {

// Helper: save 2D float array as binary (rows x cols, row-major)
bool saveMatrix(const QVector<QVector<float>>& mat, const QString& path)
{
    if (mat.isEmpty()) return false;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;

    int32_t rows = mat.size();
    int32_t cols = mat[0].size();
    file.write(reinterpret_cast<const char*>(&rows), sizeof(int32_t));
    file.write(reinterpret_cast<const char*>(&cols), sizeof(int32_t));

    for (const auto& row : mat)
        file.write(reinterpret_cast<const char*>(row.data()), cols * sizeof(float));

    return true;
}

} // namespace

bool BeatDetector::dumpStages(const AudioData& data, const QString& outputDir)
{
    if (data.samples.isEmpty()) return false;

    // Resample if needed
    QVector<float> audio = data.samples;
    int sr = data.sampleRate;
    if (sr != m_sampleRate)
    {
        double ratio = static_cast<double>(m_sampleRate) / sr;
        int newLen = static_cast<int>(audio.size() * ratio);
        audio.resize(newLen);
        for (int i = 0; i < newLen; ++i)
        {
            double srcPos = i / ratio;
            int idx = static_cast<int>(srcPos);
            double frac = srcPos - idx;
            if (idx + 1 < data.samples.size())
                audio[i] = static_cast<float>(data.samples[idx] * (1.0 - frac) + data.samples[idx + 1] * frac);
            else if (idx < data.samples.size())
                audio[i] = data.samples[idx];
            else
                audio[i] = 0.0f;
        }
        sr = m_sampleRate;
    }

    int halfFFT = m_fftSize / 2 + 1;
    int outBins = std::min(halfFFT, m_featureDim); // 272
    std::vector<float> window = hanningWindow(m_fftSize);

    // Window sum for normalization
    float windowSum = 0.0f;
    for (float w : window) windowSum += w;
    if (windowSum < 1e-8f) windowSum = 1.0f;

    // Stage containers
    QVector<QVector<float>> stftMag;       // magnitude per frame
    QVector<QVector<float>> logSpec;       // log(1+x) per frame

    for (int start = 0; start + m_fftSize <= audio.size(); start += m_hopSize)
    {
        // FFT
        std::vector<float> real(m_fftSize, 0.0f);
        std::vector<float> imag(m_fftSize, 0.0f);
        for (int i = 0; i < m_fftSize; ++i)
            real[i] = audio[start + i] * window[i];

        fft(real, imag);

        // Stage 1: Magnitude spectrum (normalized, truncated to 272)
        QVector<float> mag(outBins);
        for (int i = 0; i < outBins; ++i)
            mag[i] = std::sqrt(real[i] * real[i] + imag[i] * imag[i]) / windowSum;
        stftMag.append(mag);

        // Stage 2: Log(1+x)
        QVector<float> logFrame(outBins);
        for (int i = 0; i < outBins; ++i)
            logFrame[i] = std::log(1.0f + mag[i]);
        logSpec.append(logFrame);
    }

    // Save stages
    QDir().mkpath(outputDir);
    saveMatrix(stftMag, outputDir + "/stft_cpp.bin");
    saveMatrix(logSpec, outputDir + "/final_cpp.bin");

    qInfo("BeatDetector: Dumped 2 stages to %s (%d frames, %d bins)",
          qPrintable(outputDir), logSpec.size(), outBins);
    return true;
}
