"""
XTTS v2 GPT fine-tune for Mita (MiSide), adapted from
third_party/coqui-TTS/recipes/ljspeech/xtts_v2/train_gpt_xtts.py.

Downloads the official XTTS v2 base checkpoint (dvae.pth, mel_stats.pth,
vocab.json, model.pth) from huggingface.co/coqui/XTTS-v2 on first run.

Usage (from repo root):
  tools/xtts_finetune/.venv/Scripts/python.exe tools/xtts_finetune/train_xtts_mita.py
"""

import os
import shutil
from pathlib import Path

import soundfile as sf
import torch
import torchaudio
from huggingface_hub import hf_hub_download
from trainer import Trainer, TrainerArgs

from TTS.config.shared_configs import BaseDatasetConfig
from TTS.tts.configs.xtts_config import XttsAudioConfig
from TTS.tts.datasets import load_tts_samples
from TTS.tts.layers.xtts.trainer.gpt_trainer import GPTArgs, GPTTrainer, GPTTrainerConfig
import TTS.tts.models.xtts as xtts_module
import TTS.tts.layers.xtts.trainer.dataset as xtts_dataset_module


def _load_audio_soundfile(audiopath, sampling_rate):
    # torchaudio.load() routes through torchcodec on this torch version, and
    # torchcodec's native DLLs fail to load on this machine (no matching
    # shared FFmpeg libs). soundfile reads directly, sidestepping torchcodec.
    data, lsr = sf.read(audiopath, dtype="float32", always_2d=True)
    audio = torch.from_numpy(data.T)  # (channels, samples)
    if audio.size(0) != 1:
        audio = torch.mean(audio, dim=0, keepdim=True)
    if lsr != sampling_rate:
        audio = torchaudio.functional.resample(audio, lsr, sampling_rate)
    audio.clip_(-1, 1)
    return audio


xtts_module.load_audio = _load_audio_soundfile
xtts_dataset_module.load_audio = _load_audio_soundfile


def fetch(filename, dest_dir):
    # ModelManager's plain curl-style download 403s against HF's Xet CDN in
    # this environment; huggingface_hub's client handles it correctly.
    dest = os.path.join(dest_dir, filename)
    if not os.path.isfile(dest):
        print(f" > Downloading {filename}!")
        cached = hf_hub_download(repo_id="coqui/XTTS-v2", filename=filename)
        shutil.copy(cached, dest)
    return dest

REPO_ROOT = Path(__file__).resolve().parents[2]

RUN_NAME = "GPT_XTTS_v2.0_Mita_FT_resume31840"
PROJECT_NAME = "XTTS_trainer"
DASHBOARD_LOGGER = "tensorboard"
LOGGER_URI = None

OUT_PATH = str(REPO_ROOT / "tools" / "xtts_finetune" / "run" / "training")

# Resume from the eval-loss-best checkpoint of the first run instead of the
# stock XTTS base model. Eval loss stopped improving past this point, but a
# single "best" checkpoint isn't enough to tell where audio quality actually
# peaked by ear, so this run saves densely and keeps many snapshots for
# manual A/B listening instead of trusting one auto-picked checkpoint.
RESUME_CHECKPOINT = str(
    REPO_ROOT / "tools" / "xtts_finetune" / "run" / "training"
    / "GPT_XTTS_v2.0_Mita_FT-July-13-2026_08+09PM-5f7528a" / "best_model_31840.pth"
)

OPTIMIZER_WD_ONLY_ON_WEIGHTS = True
START_WITH_EVAL = True
BATCH_SIZE = 2
GRAD_ACUMM_STEPS = 126  # BATCH_SIZE * GRAD_ACUMM_STEPS = 252 (Coqui's recommended effective batch)

DATASETS_CONFIG_LIST = [
    BaseDatasetConfig(
        formatter="ljspeech",
        dataset_name="mita",
        path=str(REPO_ROOT / "dataset_tts"),
        meta_file_train="metadata.csv",
        language="pt",
    )
]

CHECKPOINTS_OUT_PATH = os.path.join(OUT_PATH, "XTTS_v2.0_original_model_files/")
os.makedirs(CHECKPOINTS_OUT_PATH, exist_ok=True)

DVAE_CHECKPOINT = fetch("dvae.pth", CHECKPOINTS_OUT_PATH)
MEL_NORM_FILE = fetch("mel_stats.pth", CHECKPOINTS_OUT_PATH)
TOKENIZER_FILE = fetch("vocab.json", CHECKPOINTS_OUT_PATH)
XTTS_CHECKPOINT = fetch("model.pth", CHECKPOINTS_OUT_PATH)

SPEAKER_REFERENCE = [str(REPO_ROOT / "dataset_tts" / "wavs" / "LocationDialogue Location1_10.wav")]
LANGUAGE = DATASETS_CONFIG_LIST[0].language


def main():
    model_args = GPTArgs(
        max_conditioning_length=132300,  # 6 secs
        min_conditioning_length=66150,  # 3 secs
        debug_loading_failures=False,
        max_wav_length=255995,  # ~11.6 seconds
        max_text_length=200,
        mel_norm_file=MEL_NORM_FILE,
        dvae_checkpoint=DVAE_CHECKPOINT,
        xtts_checkpoint=RESUME_CHECKPOINT,
        tokenizer_file=TOKENIZER_FILE,
        gpt_num_audio_tokens=1026,
        gpt_start_audio_token=1024,
        gpt_stop_audio_token=1025,
        gpt_use_masking_gt_prompt_approach=True,
        gpt_use_perceiver_resampler=True,
    )
    audio_config = XttsAudioConfig(sample_rate=22050, dvae_sample_rate=22050, output_sample_rate=24000)
    config = GPTTrainerConfig(
        output_path=OUT_PATH,
        model_args=model_args,
        run_name=RUN_NAME,
        project_name=PROJECT_NAME,
        run_description="GPT XTTS fine-tune on Mita (MiSide) Brazilian Portuguese dialogue",
        datasets=DATASETS_CONFIG_LIST,
        dashboard_logger=DASHBOARD_LOGGER,
        logger_uri=LOGGER_URI,
        audio=audio_config,
        batch_size=BATCH_SIZE,
        batch_group_size=48,
        eval_batch_size=BATCH_SIZE,
        num_loader_workers=4,
        eval_split_max_size=64,
        print_step=20,
        plot_step=50,
        log_model_step=500,
        save_step=3000,
        save_n_checkpoints=30,
        save_checkpoints=True,
        print_eval=False,
        optimizer="AdamW",
        optimizer_wd_only_on_weights=OPTIMIZER_WD_ONLY_ON_WEIGHTS,
        optimizer_params={"betas": [0.9, 0.96], "eps": 1e-8, "weight_decay": 1e-2},
        lr=5e-06,
        lr_scheduler="MultiStepLR",
        lr_scheduler_params={"milestones": [50000 * 18, 150000 * 18, 300000 * 18], "gamma": 0.5, "last_epoch": -1},
        test_sentences=[
            {"text": "Oie, Eu sou a Mita!", "speaker_wav": SPEAKER_REFERENCE, "language": LANGUAGE},
            {"text": "Eu perdi minha escova de cabelo, consegue me ajudar a encontrar?", "speaker_wav": SPEAKER_REFERENCE, "language": LANGUAGE},
        ],
    )

    model = GPTTrainer(config)

    train_samples, eval_samples = load_tts_samples(
        config,
        eval_split=True,
        eval_split_max_size=config.eval_split_max_size,
        eval_split_size=config.eval_split_size,
    )

    trainer = Trainer(
        TrainerArgs(
            restore_path=None,
            skip_train_epoch=False,
            start_with_eval=START_WITH_EVAL,
            grad_accum_steps=GRAD_ACUMM_STEPS,
        ),
        config,
        output_path=OUT_PATH,
        model=model,
        train_samples=train_samples,
        eval_samples=eval_samples,
    )
    trainer.fit()


if __name__ == "__main__":
    main()
