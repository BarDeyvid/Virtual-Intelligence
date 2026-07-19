"""
Generate test samples from an XTTS v2 Mita finetune checkpoint.

Usage (from repo root):
  tools/xtts_finetune/.venv/Scripts/python.exe tools/xtts_finetune/infer_test.py \
      --checkpoint tools/xtts_finetune/run/training/.../best_model.pth
"""

import argparse
import os
import shutil
from pathlib import Path

import soundfile as sf
import torch
import torchaudio

REPO_ROOT = Path(__file__).resolve().parents[2]

# Same torchcodec-avoidance patch used during training.
import TTS.tts.models.xtts as xtts_module


def _load_audio_soundfile(audiopath, sampling_rate):
    data, lsr = sf.read(audiopath, dtype="float32", always_2d=True)
    audio = torch.from_numpy(data.T)
    if audio.size(0) != 1:
        audio = torch.mean(audio, dim=0, keepdim=True)
    if lsr != sampling_rate:
        audio = torchaudio.functional.resample(audio, lsr, sampling_rate)
    audio.clip_(-1, 1)
    return audio


xtts_module.load_audio = _load_audio_soundfile

from TTS.tts.configs.xtts_config import XttsConfig
from TTS.tts.models.xtts import Xtts

BASE_FILES_DIR = REPO_ROOT / "tools" / "xtts_finetune" / "run" / "training" / "XTTS_v2.0_original_model_files"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--checkpoint", required=True)
    ap.add_argument("--ref-wav", default=str(REPO_ROOT / "dataset_tts" / "wavs" / "LocationDialogue Location1_10.wav"))
    ap.add_argument("--out-dir", default=str(REPO_ROOT / "tools" / "xtts_finetune" / "samples"))
    ap.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")
    args = ap.parse_args()

    config = XttsConfig()
    config.load_json(str(BASE_FILES_DIR / "config.json")) if (BASE_FILES_DIR / "config.json").exists() else None

    # The finetune run doesn't keep a standalone config.json; rebuild minimal
    # fields the model needs from the vocab file used during training.
    if not (BASE_FILES_DIR / "config.json").exists():
        from TTS.tts.layers.xtts.trainer.gpt_trainer import GPTArgs, GPTTrainerConfig
        from TTS.tts.configs.xtts_config import XttsAudioConfig

        model_args = GPTArgs(
            max_conditioning_length=132300,
            min_conditioning_length=66150,
            mel_norm_file=str(BASE_FILES_DIR / "mel_stats.pth"),
            dvae_checkpoint=str(BASE_FILES_DIR / "dvae.pth"),
            tokenizer_file=str(BASE_FILES_DIR / "vocab.json"),
            gpt_num_audio_tokens=1026,
            gpt_start_audio_token=1024,
            gpt_stop_audio_token=1025,
            gpt_use_masking_gt_prompt_approach=True,
            gpt_use_perceiver_resampler=True,
        )
        audio_config = XttsAudioConfig(sample_rate=22050, dvae_sample_rate=22050, output_sample_rate=24000)
        config = GPTTrainerConfig(model_args=model_args, audio=audio_config)

    print(f"loading model from {args.checkpoint} on {args.device}...")
    model = Xtts.init_from_config(config)
    model.load_checkpoint(config, checkpoint_path=args.checkpoint, vocab_path=str(BASE_FILES_DIR / "vocab.json"), eval=True, strict=False)
    model.to(args.device)

    test_lines = [
        "Oie, Eu sou a Mita!",
        "Nao vai dar certo....",
        "Eu perdi minha escova de cabelo, consegue me ajudar a encontrar?",
    ]

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    for i, text in enumerate(test_lines):
        print(f"synthesizing [{i}]: {text}")
        result = model.synthesize(text, speaker_wav=args.ref_wav, language="pt", temperature=0.7)
        wav = result["wav"]
        out_path = out_dir / f"best31840_sample_{i}.wav"
        sf.write(out_path, wav, 24000)
        print(f"  -> {out_path} ({len(wav)/24000:.2f}s)")

    print("done")


if __name__ == "__main__":
    main()
