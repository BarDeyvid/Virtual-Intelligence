import warnings
import os
import torch
from pathlib import Path
warnings.filterwarnings("ignore", category=UserWarning)
warnings.filterwarnings("ignore", category=FutureWarning)
os.environ["TOKENIZERS_PARALLELISM"] = "false"
from trainer import Trainer, TrainerArgs
from TTS.config.shared_configs import BaseDatasetConfig
from TTS.tts.configs.vits_config import VitsConfig
from TTS.tts.datasets import load_tts_samples
from TTS.tts.models.vits import CharactersConfig, Vits, VitsArgs, VitsAudioConfig
from multiprocessing import freeze_support


def main():
    
    # Limitar núcleos para evitar sobrecarga (boa prática)
    torch.set_num_threads(12)

    CURRENT_PATH = Path(__file__).parent
    RUN_NAME = "Alyssa-VITS-JA-EN"
    OUT_PATH = CURRENT_PATH
    RESTORE_PATH = "Alyssa-VITS-JA-EN-September-15-2025_06+25AM-0000000\\best_model_6330.pth"  # Deixado em branco para treinar do zero, como solicitado
    SKIP_TRAIN_EPOCH = False
    BATCH_SIZE = 64
    
    SAMPLE_RATE = 22050
    MAX_AUDIO_LEN_IN_SECONDS = 10

    # Dataset config
    # ATENÇÃO: É crucial ter um arquivo de metadados para treino e outro para validação
    # Ex: metadata_train.csv e metadata_val.csv
    # metadata_train.csv: contém 80-90% das amostras
    # metadata_val.csv: contém 10-20% das amostras restantes
    # Ambos os arquivos devem conter a coluna `language` (e.g., 'ja' ou 'en')
    alyssa_dataset_config = BaseDatasetConfig(
        formatter="ljspeech",
        dataset_name="my_dataset",
        meta_file_train="metadata_train.csv",  # Arquivo de treino corrigido
        meta_file_val="metadata_val.csv",    # Arquivo de validação separado
        path=os.path.join(CURRENT_PATH, "my_dataset"),
        language="ja", # idioma padrão
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

    # Args do modelo
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
        num_loader_workers=12,
        eval_split_max_size=256,
        print_step=50,
        plot_step=100,
        log_model_step=1000,
        save_step=5000,
        save_n_checkpoints=2,
        save_checkpoints=True,
        target_loss="loss_1",
        print_eval=False,
        use_phonemes=True,
        # Configurado para suportar múltiplos idiomas com o phonemizer espeak
        phonemizer="espeak",
        phoneme_language="ja",
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
            phonemes="a:a:i:i:u:u:e:e:o:o:N:N:k:k:s:s:h:h:m:m:j:j:r:r:w:w:g:g:z:z:d:d:b:b:p:p:t:t:ts:ts:ch:ch:sh:sh:j:j:f:f:hy:hy:by:by:py:py:ry:ry:ky:ky:gy:gy:sh:sh:j:j:ch:ch:ts:ts:a:a:i:i:u:u:e:e:o:o:ei:ei:ou:ou:ai:ai:au:au:ou:ou:a:a:e:e:i:i:o:o:u:u:e:e:a:a:i:i:u:u:o:o:r:r:n:n:m:m:g:g:z:z:d:d:b:b:p:p:t:t:k:k:s:s:h:h:f:f:v:v:z:z:j:j:th:th:dh:dh:l:l:r:r:w:w:y:y:p:p:t:t:k:k:b:b:d:d:g:g:s:s:z:z:sh:sh:zh:zh:ch:ch:j:j:th:th:dh:dh:m:m:n:n:ng:ng:l:l:r:r:y:y:w:w:a:a:e:e:i:i:o:o:u:u:ae:ae:ih:ih:uh:uh:ow:ow:aw:aw:oy:oy:ay:ay:aa:aa:ie:ie:ere:ere:oar:oar:ure:ure:ir:ir:er:er:ar:ar:aur:aur:a:a:e:e:i:i:o:o:u:u:b:b:d:d:g:g:p:p:t:t:k:k:j:j:w:w:r:r:l:l:y:y:m:m:n:n:f:f:v:v:s:s:z:z:sh:sh:th:th:dh:dh:h:h:ch:ch:zh:zh:ng:ng:wh:wh:oy:oy:aw:aw:ow:ow:ay:ay:iy:iy:uw:uw:uh:uh:eh:eh:ih:ih:ah:ah:ao:ao:a:a:o:o:e:e:i:i:u:u:ei:ei:oi:oi:au:au:ou:ou:er:er:or:or:ar:ar:ur:ur:a:a:i:i:u:u:e:e:o:o:ə:ə:ˈ:ˈ:ː:ː:ɪ:ɪ:ɛ:ɛ:ʒ:ʒ:ə:ə:ʊ:ʊ:ʃ:ʃ:ɡ:ɡ:ʔ:ʔ:d͡ʒ:d͡ʒ:m:m:n:n:ɾ:ɾ:ɲ:ɲ:s:s:ɕ:ɕ:t:t:t͡s:t͡s:t͡ɕ:t͡ɕ:w:w:z:z:b:b:d:d:ɡ:ɡ:p:p:t:t:k:k:m:m:n:n:ŋ:ŋ:l:l:r:r:j:j:w:w:h:h:s:s:ʃ:ʃ:tʃ:tʃ:dʒ:dʒ:a:a:e:e:i:i:o:o:u:u:ɯ:ɯ:aː:aː:eː:eː:iː:iː:oː:oː:uː:uː:ɯː:ɯː:β:β:ç:ç:d͡ʑ:d͡ʑ:ɟ:ɟ:ɴ:ɴ:ɸ:ɸ:ɺ:ɺ:ɾ:ɾ:ɺj:ɺj",
            is_unique=True,
            is_sorted=True,
            ),
        # Ele já lida com os fonemas por conta própria.
        test_sentences=[
            ["Kyaa! Sugoi match, motto ikuzo!", None, "ja"], # Corrigido: `language` no lugar certo
            ["Yo, this game’s lit! Let’s clutch!", None, "en"],
            ["Ne, kimi no aim, choo kakkoii!", None, "ja"],
        ],
        phoneme_cache_path=os.path.join(CURRENT_PATH, "phoneme_cache"),
        precompute_num_workers=16,
        start_by_longest=True,
        datasets=DATASETS_CONFIG_LIST,
        cudnn_benchmark=True,
        max_audio_len=SAMPLE_RATE * MAX_AUDIO_LEN_IN_SECONDS,
        mixed_precision=True,
        optimizer="AdamW",
        optimizer_params={"betas": [0.9, 0.96], "eps": 1e-8, "weight_decay": 1e-2},
        lr=0.0001,  # learning rate para treinar do zero
        lr_scheduler="MultiStepLR",
        lr_scheduler_params={"milestones": [50000, 150000, 300000], "gamma": 0.5, "last_epoch": -1}, # Milestones ajustados
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