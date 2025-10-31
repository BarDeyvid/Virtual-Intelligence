import os
import csv
import soundfile as sf

METADATA = "my_dataset/metadata_fixed.csv"
WAVS_DIR = "my_dataset/wavs"
EXPECTED_SR = 22050
EXPECTED_CHANNELS = 1
EXPECTED_PEAK_DB = -3.0

missing = []
wrong_sr = []
wrong_channels = []
wrong_peak = []

with open(METADATA, encoding="utf-8") as f:
    reader = csv.DictReader(f, delimiter="|")
    for row in reader:
        sample_id = row["sample_id"].strip()
        wav_path = os.path.join(WAVS_DIR, f"{sample_id}.wav")
        if not os.path.isfile(wav_path):
            missing.append(wav_path)
            continue
        # Check normalization
        try:
            data, sr = sf.read(wav_path)
            if sr != EXPECTED_SR:
                wrong_sr.append(wav_path)
            if len(data.shape) == 1:
                channels = 1
            else:
                channels = data.shape[1]
            if channels != EXPECTED_CHANNELS:
                wrong_channels.append(wav_path)
            peak = max(abs(data.flatten()))
            # Convert to dBFS
            import numpy as np
            if peak == 0:
                peak_db = -float('inf')
            else:
                peak_db = 20 * np.log10(peak)
            # -3dBFS is about 0.7071 amplitude
            if not (abs(peak_db - EXPECTED_PEAK_DB) < 0.5):
                wrong_peak.append((wav_path, peak_db))
        except Exception as e:
            print(f"Error reading {wav_path}: {e}")

print("\n--- Dataset Check Report ---")
if missing:
    print(f"Missing wav files ({len(missing)}):")
    for f in missing:
        print(f"  {f}")
else:
    print("No missing wav files.")
if wrong_sr:
    print(f"Wrong sample rate ({len(wrong_sr)}):")
    for f in wrong_sr:
        print(f"  {f}")
else:
    print("All files have correct sample rate.")
if wrong_channels:
    print(f"Wrong channel count ({len(wrong_channels)}):")
    for f in wrong_channels:
        print(f"  {f}")
else:
    print("All files are mono.")
if wrong_peak:
    print(f"Files not normalized to -3dBFS ({len(wrong_peak)}):")
    for f, db in wrong_peak:
        print(f"  {f} (peak: {db:.2f} dBFS)")
else:
    print("All files are normalized to -3dBFS.")
print("---------------------------\n")
