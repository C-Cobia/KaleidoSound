#include <QApplication>
#include <QFile>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <algorithm>

#include "MainWindow.h"
#include "../audio/AudioAnalyzer.h"
#include "../audio/AubioBeatDetector.h"
#include "../audio/BeatDetector.h"
#include "../audio/BeatReference.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // Debug mode: dump features for validation
    if (argc >= 4 && QString(argv[1]) == "--dump-features")
    {
        QString audioPath = argv[2];
        QString outputPath = argv[3];

        fprintf(stderr, "Loading audio: %s\n", qPrintable(audioPath));

        AudioAnalyzer analyzer;
        QString error;
        if (!analyzer.loadFile(audioPath, &error))
        {
            fprintf(stderr, "Failed to load: %s\n", qPrintable(error));
            return 1;
        }

        fprintf(stderr, "Loaded: %d samples, %d Hz, %.2f s\n",
               static_cast<int>(analyzer.audioData().samples.size()),
               analyzer.audioData().sampleRate,
               analyzer.audioData().durationSec);

        BeatDetector detector;
        fprintf(stderr, "Dumping features...\n");
        if (detector.dumpFeatures(analyzer.audioData(), outputPath))
            fprintf(stderr, "Dumped to: %s\n", qPrintable(outputPath));
        else
            fprintf(stderr, "Failed to dump features\n");

        return 0;
    }

    // Debug mode: dump activation curve as CSV
    if (argc >= 4 && QString(argv[1]) == "--dump-activation")
    {
        QString audioPath = argv[2];
        QString outputPath = argv[3];

        fprintf(stderr, "Loading: %s\n", qPrintable(audioPath));
        AudioAnalyzer analyzer;
        QString error;
        if (!analyzer.loadFile(audioPath, &error)) { fprintf(stderr, "Load failed: %s\n", qPrintable(error)); return 1; }

        BeatDetector detector;
        QString appDir = QCoreApplication::applicationDirPath();
        for (const QString& path : {appDir+"/Third-Parties/beatnet-models/beatnet_crnn.pt",
                                     appDir+"/../Third-Parties/beatnet-models/beatnet_crnn.pt",
                                     appDir+"/../../Third-Parties/beatnet-models/beatnet_crnn.pt"})
        { if (QFile::exists(path) && detector.loadModel(path)) break; }

        BeatInfo info = detector.detect(analyzer.audioData());

        QFile outFile(outputPath);
        if (outFile.open(QIODevice::WriteOnly))
        {
            outFile.write("frame,time,activation\n");
            double fps = info.activationSampleRate;
            for (int i = 0; i < info.activation.size(); ++i)
            {
                double t = (fps > 0) ? i / fps : 0.0;
                outFile.write(QByteArray::number(i) + "," +
                              QByteArray::number(t, 'f', 6) + "," +
                              QByteArray::number(info.activation[i], 'f', 8) + "\n");
            }
            fprintf(stderr, "Saved %d frames to %s\n", static_cast<int>(info.activation.size()), qPrintable(outputPath));
        }
        return 0;
    }

    // Debug mode: dump all intermediate stages
    if (argc >= 4 && QString(argv[1]) == "--dump-stages")
    {
        QString audioPath = argv[2];
        QString outputDir = argv[3];

        fprintf(stderr, "Loading audio: %s\n", qPrintable(audioPath));

        AudioAnalyzer analyzer;
        QString error;
        if (!analyzer.loadFile(audioPath, &error))
        {
            fprintf(stderr, "Failed to load: %s\n", qPrintable(error));
            return 1;
        }

        fprintf(stderr, "Loaded: %d samples, %d Hz, %.2f s\n",
               static_cast<int>(analyzer.audioData().samples.size()),
               analyzer.audioData().sampleRate,
               analyzer.audioData().durationSec);

        BeatDetector detector;
        fprintf(stderr, "Dumping stages to: %s\n", qPrintable(outputDir));
        if (detector.dumpStages(analyzer.audioData(), outputDir))
            fprintf(stderr, "Done!\n");
        else
            fprintf(stderr, "Failed to dump stages\n");

        return 0;
    }

    // Debug mode: export BeatNet/Aubio beat analysis data for offline inspection
    if (argc >= 4 && QString(argv[1]) == "--dump-beat-debug")
    {
        QString audioPath = argv[2];
        QString outputDir = argv[3];
        QDir().mkpath(outputDir);

        AudioAnalyzer analyzer;
        QString error;
        if (!analyzer.loadFile(audioPath, &error))
        {
            fprintf(stderr, "Failed to load: %s\n", qPrintable(error));
            return 1;
        }

        BeatDetector detector;
        QString appDir = QCoreApplication::applicationDirPath();
        QStringList modelPaths = {
            appDir + "/Third-Parties/beatnet-models/beatnet_crnn.pt",
            appDir + "/../Third-Parties/beatnet-models/beatnet_crnn.pt",
            appDir + "/../../Third-Parties/beatnet-models/beatnet_crnn.pt",
        };
        bool loaded = false;
        for (const QString& path : modelPaths)
        {
            if (QFile::exists(path) && detector.loadModel(path))
            {
                loaded = true;
                break;
            }
        }
        if (!loaded)
        {
            fprintf(stderr, "No BeatNet model found\n");
            return 1;
        }

        BeatInfo beatnet = detector.detect(analyzer.audioData());
        AubioBeatDetector fallback;
        BeatAnalysisResult aubio = fallback.analyze(analyzer.audioData());
        QVector<BeatEvent> referenceBeats;
        if (argc >= 5)
            referenceBeats = BeatReference::loadFromFile(argv[4]);
        else
            referenceBeats = BeatReference::loadSidecarForAudio(audioPath);

        QFile activationFile(outputDir + "/activation.csv");
        if (activationFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream out(&activationFile);
            out << "time,beat_activation,downbeat_activation\n";
            for (int i = 0; i < beatnet.activation.size(); ++i)
            {
                const double timeSec = beatnet.activationSampleRate > 0.0 ? i / beatnet.activationSampleRate : 0.0;
                const float downbeat = i < beatnet.downbeatActivation.size() ? beatnet.downbeatActivation[i] : 0.0f;
                out << timeSec << "," << beatnet.activation[i] << "," << downbeat << "\n";
            }
        }

        QFile candidateFile(outputDir + "/candidates.csv");
        if (candidateFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream out(&candidateFile);
            out << "time,score\n";
            for (int i = 0; i < beatnet.candidatePeakTimes.size(); ++i)
            {
                const float score = i < beatnet.candidatePeakScores.size() ? beatnet.candidatePeakScores[i] : 0.0f;
                out << beatnet.candidatePeakTimes[i] << "," << score << "\n";
            }
        }

        QJsonObject root;
        root["audio"] = audioPath;
        root["beatnet_bpm"] = beatnet.bpm;
        root["aubio_bpm"] = aubio.bpm;
        root["attack_phase_offset_ms"] = beatnet.attackPhaseOffsetSec * 1000.0;
        root["perceptual_shift_ms"] = beatnet.perceptualShiftSec * 1000.0;
        root["attack_phase_matches"] = beatnet.attackPhaseMatches;

        QJsonArray beatnetBeats;
        for (int i = 0; i < beatnet.beatTimes.size(); ++i)
        {
            QJsonObject beat;
            beat["time"] = beatnet.beatTimes[i];
            beat["confidence"] = i < beatnet.beatConfidences.size() ? beatnet.beatConfidences[i] : 1.0;
            beat["downbeat"] = i < beatnet.beatIsDownbeat.size() && beatnet.beatIsDownbeat[i];
            beatnetBeats.append(beat);
        }
        root["beatnet_beats"] = beatnetBeats;

        QJsonArray aubioBeats;
        for (const auto& event : aubio.beats)
        {
            QJsonObject beat;
            beat["time"] = event.timeSec;
            beat["confidence"] = event.confidence;
            aubioBeats.append(beat);
        }
        root["aubio_beats"] = aubioBeats;

        QJsonArray reference;
        for (const auto& event : referenceBeats)
        {
            QJsonObject beat;
            beat["time"] = event.timeSec;
            beat["downbeat"] = event.isDownbeat;
            reference.append(beat);
        }
        root["reference_beats"] = reference;

        QJsonObject shiftVariants;
        const QVector<double> shifts = { -0.020, -0.035, -0.050 };
        for (double shift : shifts)
        {
            QJsonArray shifted;
            for (int i = 0; i < beatnet.beatTimes.size(); ++i)
            {
                QJsonObject beat;
                beat["time"] = std::max(0.0, beatnet.beatTimes[i] + shift);
                beat["downbeat"] = i < beatnet.beatIsDownbeat.size() && beatnet.beatIsDownbeat[i];
                shifted.append(beat);
            }
            shiftVariants[QStringLiteral("%1ms").arg(static_cast<int>(shift * 1000.0))] = shifted;
        }
        root["perceptual_shift_variants"] = shiftVariants;

        QFile beatsFile(outputDir + "/beats.json");
        if (beatsFile.open(QIODevice::WriteOnly))
            beatsFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));

        fprintf(stderr, "Beat debug exported to: %s\n", qPrintable(outputDir));
        return 0;
    }

    // Debug mode: test BeatNet inference end-to-end
    if (argc >= 3 && QString(argv[1]) == "--test-inference")
    {
        QString audioPath = argv[2];

        fprintf(stderr, "Loading audio: %s\n", qPrintable(audioPath));

        AudioAnalyzer analyzer;
        QString error;
        if (!analyzer.loadFile(audioPath, &error))
        {
            fprintf(stderr, "Failed to load: %s\n", qPrintable(error));
            return 1;
        }

        fprintf(stderr, "Loaded: %d samples, %d Hz, %.2f s\n",
               static_cast<int>(analyzer.audioData().samples.size()),
               analyzer.audioData().sampleRate,
               analyzer.audioData().durationSec);

        // Try BeatNet
        BeatDetector detector;
        QString appDir = QCoreApplication::applicationDirPath();
        QStringList modelPaths = {
            appDir + "/Third-Parties/beatnet-models/beatnet_crnn.pt",
            appDir + "/../Third-Parties/beatnet-models/beatnet_crnn.pt",
            appDir + "/../../Third-Parties/beatnet-models/beatnet_crnn.pt",
        };
        bool loaded = false;
        for (const QString& path : modelPaths)
        {
            if (QFile::exists(path) && detector.loadModel(path))
            {
                fprintf(stderr, "Model loaded: %s\n", qPrintable(path));
                loaded = true;
                break;
            }
        }

        if (!loaded)
        {
            fprintf(stderr, "No BeatNet model found\n");
            return 1;
        }

        fprintf(stderr, "Running inference...\n");
        BeatInfo result = detector.detect(analyzer.audioData());

        fprintf(stderr, "BPM: %.1f\n", result.bpm);
        fprintf(stderr, "Beats: %d\n", static_cast<int>(result.beatTimes.size()));
        if (!result.activation.isEmpty())
        {
            float minAct = result.activation[0];
            float maxAct = result.activation[0];
            double sumAct = 0.0;
            for (float value : result.activation)
            {
                minAct = std::min(minAct, value);
                maxAct = std::max(maxAct, value);
                sumAct += value;
            }
            fprintf(stderr, "Activation: frames=%d min=%.6f mean=%.6f max=%.6f fps=%.2f\n",
                    static_cast<int>(result.activation.size()),
                    minAct,
                    sumAct / result.activation.size(),
                    maxAct,
                    result.activationSampleRate);
        }
        if (!result.downbeatActivation.isEmpty())
        {
            float minAct = result.downbeatActivation[0];
            float maxAct = result.downbeatActivation[0];
            double sumAct = 0.0;
            for (float value : result.downbeatActivation)
            {
                minAct = std::min(minAct, value);
                maxAct = std::max(maxAct, value);
                sumAct += value;
            }
            fprintf(stderr, "Downbeat: frames=%d min=%.6f mean=%.6f max=%.6f\n",
                    static_cast<int>(result.downbeatActivation.size()),
                    minAct,
                    sumAct / result.downbeatActivation.size(),
                    maxAct);
        }
        if (!result.activation.isEmpty())
        {
            QVector<int> topFrames;
            topFrames.reserve(result.activation.size());
            for (int i = 0; i < result.activation.size(); ++i)
                topFrames.append(i);
            std::sort(topFrames.begin(), topFrames.end(), [&result](int a, int b) {
                return result.activation[a] > result.activation[b];
            });
            fprintf(stderr, "Top activation frames:");
            for (int i = 0; i < topFrames.size() && i < 8; ++i)
            {
                const int frame = topFrames[i];
                const double timeSec = result.activationSampleRate > 0.0
                    ? frame / result.activationSampleRate
                    : 0.0;
                fprintf(stderr, " %.3fs=%.4f", timeSec, result.activation[frame]);
            }
            fprintf(stderr, "\n");
        }
        for (int i = 0; i < result.beatTimes.size() && i < 20; ++i)
        {
            const bool isDownbeat = i < result.beatIsDownbeat.size() && result.beatIsDownbeat[i];
            fprintf(stderr, "  beat[%d]: %.3f s%s\n", i, result.beatTimes[i], isDownbeat ? " downbeat" : "");
        }
        if (result.beatTimes.size() > 20)
            fprintf(stderr, "  ... (%d more)\n", static_cast<int>(result.beatTimes.size()) - 20);

        if (result.beatTimes.isEmpty())
        {
            AubioBeatDetector fallback;
            BeatAnalysisResult aubio = fallback.analyze(analyzer.audioData());
            fprintf(stderr, "Aubio fallback: success=%d BPM=%.1f Beats=%d\n",
                    aubio.success ? 1 : 0,
                    aubio.bpm,
                    aubio.beatCount());
            for (int i = 0; i < aubio.beats.size() && i < 20; ++i)
                fprintf(stderr, "  aubio[%d]: %.3f s\n", i, aubio.beats[i].timeSec);
            if (aubio.beats.size() > 20)
                fprintf(stderr, "  ... (%d more)\n", static_cast<int>(aubio.beats.size()) - 20);
        }

        // === Tempo Diagnostics ===
        fprintf(stderr, "\n=== Tempo Diagnostics ===\n");
        fprintf(stderr, "Selected tempo: %.3f s (%.1f BPM)\n", result.selectedTempoSec, result.selectedBpm);
        fprintf(stderr, "Tempo score: %.6f\n", result.tempoScore);
        fprintf(stderr, "Candidates: %d\n", result.candidateCount);
        fprintf(stderr, "Median candidate IBI: %.3f s (%.1f BPM)\n",
                result.medianCandidateInterval,
                result.medianCandidateInterval > 0 ? 60.0 / result.medianCandidateInterval : 0.0);

        if (!result.intervalHistogram.isEmpty())
        {
            fprintf(stderr, "Interval histogram (%d bins, %.2f-%.2f s):\n",
                    static_cast<int>(result.intervalHistogram.size()),
                    result.histogramMinSec, result.histogramMaxSec);

            // Find top-5 peaks in histogram
            struct HistPeak { int bin; double value; double bpm; };
            QVector<HistPeak> peaks;
            for (int i = 0; i < result.intervalHistogram.size(); ++i)
            {
                double ibi = result.histogramMinSec + i * (result.histogramMaxSec - result.histogramMinSec) / result.intervalHistogram.size();
                peaks.append({i, result.intervalHistogram[i], 60.0 / ibi});
            }
            std::sort(peaks.begin(), peaks.end(), [](const HistPeak& a, const HistPeak& b) { return a.value > b.value; });

            fprintf(stderr, "  Top-5 tempo hypotheses:\n");
            for (int i = 0; i < std::min(5, static_cast<int>(peaks.size())); ++i)
            {
                double ibi = result.histogramMinSec + peaks[i].bin * (result.histogramMaxSec - result.histogramMinSec) / result.intervalHistogram.size();
                fprintf(stderr, "    #%d: IBI=%.3fs BPM=%.1f score=%.4f%s\n",
                        i + 1, ibi, peaks[i].bpm, peaks[i].value,
                        std::abs(peaks[i].bpm - result.selectedBpm) < 1.0 ? " [SELECTED]" : "");
            }

            // Check half-time / double-time
            fprintf(stderr, "  Half-time/double-time check:\n");
            for (int i = 0; i < std::min(5, static_cast<int>(peaks.size())); ++i)
            {
                double ibi = result.histogramMinSec + peaks[i].bin * (result.histogramMaxSec - result.histogramMinSec) / result.intervalHistogram.size();
                double bpm = 60.0 / ibi;
                double halfBpm = bpm / 2.0;
                double doubleBpm = bpm * 2.0;
                fprintf(stderr, "    BPM=%.1f  half=%.1f  double=%.1f\n", bpm, halfBpm, doubleBpm);
            }
        }

        return 0;
    }

    QFile styleFile(QStringLiteral(":/styles/app.qss"));
    if (styleFile.open(QIODeviceBase::ReadOnly))
    {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }

    MainWindow window;
    window.show();

    return app.exec();
}
