#include "AubioBeatDetector.h"

#include <aubio/aubio.h>
#include <algorithm>
#include <cmath>

namespace {

constexpr double kMinBeatSpacingSec = 0.15;

bool appendBeatIfDistinct(QVector<BeatEvent>& beats, double timeSec, double durationSec)
{
    if (!std::isfinite(timeSec) || timeSec < 0.0 || timeSec > durationSec)
        return false;

    if (!beats.isEmpty() && std::abs(beats.back().timeSec - timeSec) < kMinBeatSpacingSec)
        return false;

    return true;
}

} // namespace

BeatAnalysisResult AubioBeatDetector::analyze(const AudioData& audio)
{
    BeatAnalysisResult result;
    result.method = "aubio";

    if (audio.samples.isEmpty())
        return result;

    const uint_t winSize = 2048;
    const uint_t hopSize = 512;
    const uint_t sr = static_cast<uint_t>(audio.sampleRate);

    aubio_tempo_t* tempo = new_aubio_tempo("default", winSize, hopSize, sr);
    if (!tempo)
        return result;

    aubio_tempo_set_silence(tempo, -60.0f);
    aubio_tempo_set_threshold(tempo, 0.15f);

    fvec_t* input = new_fvec(hopSize);
    fvec_t* tempoOut = new_fvec(1);

    int totalSamples = audio.samples.size();
    int pos = 0;

    while (pos < totalSamples)
    {
        for (uint_t i = 0; i < hopSize; ++i)
        {
            const int idx = pos + static_cast<int>(i);
            const float sample = idx < totalSamples ? audio.samples[idx] : 0.0f;
            fvec_set_sample(input, sample, i);
        }

        aubio_tempo_do(tempo, input, tempoOut);
        if (fvec_get_sample(tempoOut, 0) != 0.0f)
        {
            const double timeSec = static_cast<double>(aubio_tempo_get_last_s(tempo));
            if (appendBeatIfDistinct(result.beats, timeSec, audio.durationSec))
            {
                BeatEvent event;
                event.timeSec = timeSec;
                event.confidence = 1.0f;
                result.beats.append(event);
            }
        }

        pos += static_cast<int>(hopSize);
    }

    result.bpm = static_cast<double>(aubio_tempo_get_bpm(tempo));

    // Fallback BPM from median interval
    if ((!std::isfinite(result.bpm) || result.bpm <= 0.0) && result.beats.size() >= 2)
    {
        QVector<double> intervals;
        for (int i = 1; i < result.beats.size(); ++i)
        {
            double interval = result.beats[i].timeSec - result.beats[i - 1].timeSec;
            if (interval > 0.15 && interval < 2.0)
                intervals.append(interval);
        }
        if (!intervals.isEmpty())
        {
            std::sort(intervals.begin(), intervals.end());
            result.bpm = 60.0 / intervals[intervals.size() / 2];
        }
    }

    del_aubio_tempo(tempo);
    del_fvec(input);
    del_fvec(tempoOut);

    result.success = !result.beats.isEmpty();
    return result;
}
