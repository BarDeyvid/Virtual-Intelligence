import os
import csv
from pydub import AudioSegment
from datetime import datetime

RAW_DIR = "raw"
OUT_DIR = "my_dataset/wavs"
METADATA_CSV = "metadata.csv"

# Ensure output directory exists
os.makedirs(OUT_DIR, exist_ok=True)

# Read metadata and preserve order
with open(METADATA_CSV, encoding="utf-8") as f:
    reader = csv.DictReader(f, delimiter="|")
    metadata = [row for row in reader]


# Get all mp3 files in raw folder with their modification times
raw_files = []
for fname in os.listdir(RAW_DIR):
    if not fname.lower().endswith('.mp3'):
        continue
    fpath = os.path.join(RAW_DIR, fname)
    if os.path.isfile(fpath):
        mtime = os.path.getmtime(fpath)
        raw_files.append((fname, mtime))

# Sort files from oldest to newest
raw_files.sort(key=lambda x: x[1])


# Map sample_id to mp3 files by order (oldest to newest)
sample_to_file = {}
num_files = min(len(metadata), len(raw_files))
for i in range(num_files):
    sid = metadata[i]["sample_id"]
    fname, _ = raw_files[i]
    sample_to_file[sid] = fname

# Process in the order of metadata (which is the order in the CSV)
for sample in metadata:
    sid = sample["sample_id"]
    if sid not in sample_to_file:
        print(f"Warning: No file found for {sid}")
        continue

    in_path = os.path.join(RAW_DIR, sample_to_file[sid])
    out_path = os.path.join(OUT_DIR, f"{sid}.wav")
    # Load audio (force mp3)
    audio = AudioSegment.from_file(in_path, format="mp3")
    # Convert to mono, 22kHz
    audio = audio.set_channels(1).set_frame_rate(22050)
    # Normalize to -3dBFS
    change_in_dBFS = -3.0 - audio.max_dBFS
    audio = audio.apply_gain(change_in_dBFS)
    # Export as wav
    audio.export(out_path, format="wav")
    print(f"Processed {sid} -> {out_path}")

print("All done.")