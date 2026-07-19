"""
Quick sanity-check inference against an in-progress Mita finetune checkpoint.
Adapted from third_party/StyleTTS2/Demo/Inference_LibriTTS.ipynb, swapping the
English phonemizer/textcleaner for the Kokoro-vocab pt-br pipeline already
wired into this repo's vendored StyleTTS2 copy.

Usage (from repo root):
  tools/kokoro_finetune/.venv/Scripts/python.exe tools/kokoro_finetune/infer_test.py \
      --checkpoint third_party/StyleTTS2/Models/Mita/epoch_2nd_00004.pth
"""

import argparse
import os
import sys
from pathlib import Path
from collections import OrderedDict

import numpy as np
import soundfile as sf
import torch
import torchaudio
import yaml

REPO_ROOT = Path(__file__).resolve().parents[2]
ST2_DIR = REPO_ROOT / "third_party" / "StyleTTS2"
sys.path.insert(0, str(ST2_DIR))
ORIGINAL_CWD = Path.cwd()

os.environ.setdefault("PHONEMIZER_ESPEAK_LIBRARY", str((REPO_ROOT / "third_party/espeak-ng/libespeak-ng.dll").resolve()))
os.environ.setdefault("ESPEAK_DATA_PATH", str((REPO_ROOT / "third_party/espeak-ng/espeak-ng-data").resolve()))

os.chdir(ST2_DIR)  # StyleTTS2 modules assume cwd == repo, for relative Utils/ paths

from models import build_model, load_ASR_models, load_F0_models
from Utils.PLBERT.util import load_plbert
from Modules.diffusion.sampler import DiffusionSampler, ADPM2Sampler, KarrasSchedule
from text_utils import TextCleaner
from munch import Munch
from phonemizer.backend import EspeakBackend

DEVICE = "cpu"  # forced: training already saturates GPU VRAM, avoid contention/OOM

to_mel = torchaudio.transforms.MelSpectrogram(n_mels=80, n_fft=2048, win_length=1200, hop_length=300)
MEAN, STD = -4, 4


def preprocess(wave):
    wave_tensor = torch.from_numpy(wave).float()
    mel_tensor = to_mel(wave_tensor)
    return (torch.log(1e-5 + mel_tensor.unsqueeze(0)) - MEAN) / STD


def length_to_mask(lengths):
    mask = torch.arange(lengths.max()).unsqueeze(0).expand(lengths.shape[0], -1).type_as(lengths)
    return torch.gt(mask + 1, lengths.unsqueeze(1))


def recursive_munch(d):
    if isinstance(d, dict):
        return Munch((k, recursive_munch(v)) for k, v in d.items())
    if isinstance(d, list):
        return [recursive_munch(v) for v in d]
    return d


def load_model(config_path, checkpoint_path):
    config = yaml.safe_load(open(config_path))

    text_aligner = load_ASR_models(config["ASR_path"], config["ASR_config"])
    pitch_extractor = load_F0_models(config["F0_path"])
    plbert = load_plbert(config["PLBERT_dir"])

    model_params = recursive_munch(config["model_params"])
    model = build_model(model_params, text_aligner, pitch_extractor, plbert)
    _ = [model[key].eval() for key in model]
    _ = [model[key].to(DEVICE) for key in model]

    state = torch.load(checkpoint_path, map_location="cpu", weights_only=False)
    params = state["net"]
    for key in model:
        if key not in params:
            continue
        try:
            model[key].load_state_dict(params[key])
        except Exception:
            new_state_dict = OrderedDict((k[7:], v) for k, v in params[key].items())
            model[key].load_state_dict(new_state_dict, strict=False)
    _ = [model[key].eval() for key in model]

    sampler = DiffusionSampler(
        model.diffusion.diffusion,
        sampler=ADPM2Sampler(),
        sigma_schedule=KarrasSchedule(sigma_min=0.0001, sigma_max=3.0, rho=9.0),
        clamp=False,
    )
    return model, model_params, sampler


def compute_style(model, wav_path):
    import librosa
    wave, sr = librosa.load(wav_path, sr=24000)
    audio, _ = librosa.effects.trim(wave, top_db=30)
    mel_tensor = preprocess(audio).to(DEVICE)
    with torch.no_grad():
        ref_s = model.style_encoder(mel_tensor.unsqueeze(1))
        ref_p = model.predictor_encoder(mel_tensor.unsqueeze(1))
    return torch.cat([ref_s, ref_p], dim=1)


def phonemize(text, backend, text_cleaner):
    ph = backend.phonemize([text])[0]
    tokens = text_cleaner(ph)
    tokens.insert(0, 0)
    return torch.LongTensor(tokens).to(DEVICE).unsqueeze(0)


def synthesize(model, model_params, sampler, text, ref_s, backend, text_cleaner, alpha=1.0, beta=1.0, diffusion_steps=5, embedding_scale=1):
    tokens = phonemize(text, backend, text_cleaner)
    with torch.no_grad():
        input_lengths = torch.LongTensor([tokens.shape[-1]]).to(DEVICE)
        text_mask = length_to_mask(input_lengths).to(DEVICE)

        t_en = model.text_encoder(tokens, input_lengths, text_mask)
        bert_dur = model.bert(tokens, attention_mask=(~text_mask).int())
        d_en = model.bert_encoder(bert_dur).transpose(-1, -2)

        s_pred = sampler(
            noise=torch.randn((1, 256)).unsqueeze(1).to(DEVICE),
            embedding=bert_dur,
            embedding_scale=embedding_scale,
            features=ref_s,
            num_steps=diffusion_steps,
        ).squeeze(1)

        s = s_pred[:, 128:]
        ref = s_pred[:, :128]
        ref = alpha * ref + (1 - alpha) * ref_s[:, :128]
        s = beta * s + (1 - beta) * ref_s[:, 128:]

        d = model.predictor.text_encoder(d_en, s, input_lengths, text_mask)
        x, _ = model.predictor.lstm(d)
        duration = model.predictor.duration_proj(x)
        duration = torch.sigmoid(duration).sum(axis=-1)
        pred_dur = torch.round(duration.squeeze()).clamp(min=1)

        pred_aln_trg = torch.zeros(input_lengths, int(pred_dur.sum().data))
        c_frame = 0
        for i in range(pred_aln_trg.size(0)):
            pred_aln_trg[i, c_frame:c_frame + int(pred_dur[i].data)] = 1
            c_frame += int(pred_dur[i].data)

        en = d.transpose(-1, -2) @ pred_aln_trg.unsqueeze(0).to(DEVICE)
        asr = t_en @ pred_aln_trg.unsqueeze(0).to(DEVICE)
        F0_pred, N_pred = model.predictor.F0Ntrain(en, s)

        out = model.decoder(asr, F0_pred, N_pred, ref.squeeze().unsqueeze(0))
    return out.squeeze().cpu().numpy()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--checkpoint", required=True)
    ap.add_argument("--config", default="Configs/config_ft_mita.yml")
    ap.add_argument("--ref-wav", default="../../dataset_tts/wavs/LocationDialogue Location1_10.wav")
    ap.add_argument("--out-dir", default="tools/kokoro_finetune/samples")
    args = ap.parse_args()

    args.checkpoint = str((ORIGINAL_CWD / args.checkpoint).resolve())
    args.out_dir = str((ORIGINAL_CWD / args.out_dir).resolve())

    print(f"loading model from {args.checkpoint} on {DEVICE}...")
    model, model_params, sampler = load_model(args.config, args.checkpoint)

    print("computing reference style from", args.ref_wav)
    ref_s = compute_style(model, args.ref_wav)

    backend = EspeakBackend("pt-br", preserve_punctuation=True, with_stress=True)
    text_cleaner = TextCleaner()

    test_lines = [
        "Oie, Eu sou a Mita!",
        "Nao vai dar certo....",
        "Eu perdi minha escova de cabelo, consegue me ajudar a encontrar?",
    ]

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    for i, text in enumerate(test_lines):
        print(f"synthesizing [{i}]: {text}")
        wav = synthesize(model, model_params, sampler, text, ref_s, backend, text_cleaner)
        print(f"  raw stats: min={wav.min():.4f} max={wav.max():.4f} std={wav.std():.4f}")
        out_path = out_dir / f"sample_{i}.wav"
        sf.write(out_path, wav, 24000, subtype="FLOAT")
        print(f"  -> {out_path} ({len(wav)/24000:.2f}s)")

    print("done")


if __name__ == "__main__":
    main()
