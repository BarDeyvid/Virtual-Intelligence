from TTS.api import TTS
tts = TTS(model_name="tts_models/multilingual/multi-dataset/xtts_v2", gpu=True)
tts.tts_to_file(
    text="Kyaa! Sugoi match, motto ikuzo!",
    speaker_wav="my_dataset\wavs\sample011.wav",
    language="ja",
    file_path="test_xtts_jp.wav"
)