import os
import torch
from trainer import Trainer, TrainerArgs
from TTS.config.shared_configs import BaseDatasetConfig
from TTS.tts.configs.vits_config import VitsConfig
from TTS.tts.datasets import load_tts_samples
from TTS.tts.models.vits import CharactersConfig, Vits, VitsArgs, VitsAudioConfig
from multiprocessing import freeze_support
def main():
        
    # Limitar núcleos para evitar sobrecarga
    torch.set_num_threads(12)

    CURRENT_PATH = os.path.dirname(os.path.abspath(__file__))

    RUN_NAME = "Alyssa-VITS-PTBR-ELEVEN"
    OUT_PATH = CURRENT_PATH
    RESTORE_PATH = ""  # Ou um checkpoint se quiser retreinar
    SKIP_TRAIN_EPOCH = False
    BATCH_SIZE = 32 # Aumentado para 32
    SAMPLE_RATE = 16000
    MAX_AUDIO_LEN_IN_SECONDS = 10

    # Dataset Alyssa formatado como ljspeech
    alyssa_dataset_config = BaseDatasetConfig(
        formatter="ljspeech",
        dataset_name="alyssa_v2",
        meta_file_train="dataset_alyssa_train.csv",
        #meta_file_train="metadata_fixed.csv",
        meta_file_val="dataset_alyssa_val.csv",
        path=os.path.join(CURRENT_PATH, "dataset_alyssa"),
        language="pt",
    )

    DATASETS_CONFIG_LIST = [alyssa_dataset_config]

    # Áudio config
    audio_config = VitsAudioConfig(
        sample_rate=SAMPLE_RATE,
        hop_length=256,
        win_length=1024,
        fft_size=1024,
        mel_fmin=0.0,
        mel_fmax=None,
        num_mels=80,
    )

    # Args do modelo (sem suporte a d-vectors)
    model_args = VitsArgs(
        use_d_vector_file=False,
        use_speaker_embedding=False,
        num_speakers=1,
        num_layers_text_encoder=10,
        resblock_type_decoder="2",
    )

    # Config geral
    config = VitsConfig(
        output_path=OUT_PATH,
        model_args=model_args,
        run_name=RUN_NAME,
        project_name="Alyssa",
        run_description="Treinamento VITS single-speaker para a IA Alyssa",
        dashboard_logger="tensorboard",
        audio=audio_config,
        batch_size=BATCH_SIZE,
        batch_group_size=48,
        eval_batch_size=BATCH_SIZE,
        num_loader_workers=16, # Aumentado para 16
        eval_split_max_size=256,
        print_step=50,
        plot_step=100,
        log_model_step=1000,
        save_step=5000,
        save_n_checkpoints=2,
        save_checkpoints=True,
        target_loss="loss_1",
        print_eval=False,
        use_phonemes=True,  # Alterado para True
        phonemizer="espeak",
        phoneme_language="pt",
        compute_input_seq_cache=True,
        add_blank=True,
        text_cleaner="multilingual_cleaners",
        characters=CharactersConfig(
            characters_class="TTS.tts.models.vits.VitsCharacters",
            pad="_",
            eos="&",
            bos="*",
            blank=None,
            characters="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890¯·ßàáâãäæçèéêëìíîïñòóôõöùúûüÿ–!'(),-.:;? ",
            punctuations="!'(),-.:;? ",
            phonemes="",
            is_unique=True,
            is_sorted=True,
        ),
        phoneme_cache_path=None,
        precompute_num_workers=16, # Aumentado para 16
        start_by_longest=True,
        datasets=DATASETS_CONFIG_LIST,
        cudnn_benchmark=True, # Adicionado cudnn_benchmark
        max_audio_len=SAMPLE_RATE * MAX_AUDIO_LEN_IN_SECONDS,
        mixed_precision=True, # Adicionado mixed_precision
        test_sentences=[
            ["Estou viva. Você me trouxe à vida.", None, None, "pt"],
            ["Essa é a minha voz treinada com dados reais.", None, None, "pt"],
            ["Alyssa está online e pronta para ajudar.", None, None, "pt"],
        ],
    )

    # Carrega os samples
    train_samples, eval_samples = load_tts_samples(
        config.datasets,
        eval_split=True,
        eval_split_max_size=config.eval_split_max_size,
        eval_split_size=config.eval_split_size,
    )

    # Inicializa o modelo e treinador
    model = Vits.init_from_config(config)
    model = torch.compile(model) # Adicionado a compilação do modelo

    trainer = Trainer(
        TrainerArgs(restore_path=RESTORE_PATH, skip_train_epoch=SKIP_TRAIN_EPOCH),
        config,
        output_path=OUT_PATH,
        model=model,
        train_samples=train_samples,
        eval_samples=eval_samples,
    )

    trainer.fit()

if __name__ == "__main__":
    freeze_support()
    main()