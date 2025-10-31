import os
import csv
from dotenv import load_dotenv
from elevenlabs.client import ElevenLabs
from pydub import AudioSegment

# Config
METADATA = "my_dataset/metadata_fixed.csv"
WAVS_DIR = "my_dataset/wavs"
TMP_DIR = "tmp_elevenlabs"
VOICE_ID = "JBFqnCBsd6RMkjVDRZzb"
MODEL_ID = "eleven_multilingual_v2"
OUTPUT_FORMAT = "mp3_44100_128"

os.makedirs(WAVS_DIR, exist_ok=True)
os.makedirs(TMP_DIR, exist_ok=True)
load_dotenv()
elevenlabs = ElevenLabs(api_key="sk_8db772bd93ffc8f13f0435f1771aed52d7dd04bcc34f8c81")

with open(METADATA, encoding="utf-8") as f:
    reader = csv.DictReader(f, delimiter="|")
    for row in reader:
        sample_id = row["sample_id"].strip()
        text = row["transcription"].strip()
        wav_path = os.path.join(WAVS_DIR, f"{sample_id}.wav")
        if os.path.isfile(wav_path):
            print(f"Exists: {wav_path}")
            continue

        print(f"Generating: {sample_id} - {text}")
        # Generate audio with ElevenLabs
        audio = elevenlabs.text_to_speech.convert(
            text=text,
            voice_id=VOICE_ID,
            model_id=MODEL_ID,
            output_format=OUTPUT_FORMAT,
        )
        mp3_path = os.path.join(TMP_DIR, f"{sample_id}.mp3")
        with open(mp3_path, "wb") as out_f:
            for chunk in audio:
                out_f.write(chunk)

        # Normalize and convert to wav
        audio_seg = AudioSegment.from_file(mp3_path, format="mp3")
        audio_seg = audio_seg.set_channels(1).set_frame_rate(22050)
        change_in_dBFS = -3.0 - audio_seg.max_dBFS
        audio_seg = audio_seg.apply_gain(change_in_dBFS)
        audio_seg.export(wav_path, format="wav")
        print(f"Saved: {wav_path}")

print("All samples processed.")