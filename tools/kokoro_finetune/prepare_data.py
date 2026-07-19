"""
Phonemizes dataset_tts/train_clean.csv into the wav|phonemes|speaker_id format
StyleTTS2's FilePathDataset expects, using Kokoro's own IPA vocab (not
StyleTTS2's default English-only symbol table) so token ids line up with the
pretrained Kokoro checkpoint's embeddings.

Writes:
  dataset_tts/train_list.txt
  dataset_tts/val_list.txt
  dataset_tts/OOD_texts.txt
"""

import csv
import json
import os
import random
from pathlib import Path

os.environ.setdefault(
    "PHONEMIZER_ESPEAK_LIBRARY",
    str(Path("third_party/espeak-ng/libespeak-ng.dll").resolve()),
)
os.environ.setdefault(
    "ESPEAK_DATA_PATH",
    str(Path("third_party/espeak-ng/espeak-ng-data").resolve()),
)

from phonemizer.backend import EspeakBackend

VAL_SIZE = 40
SPEAKER_ID = 0


def load_kokoro_vocab():
    with open("models/kokoro/config.json", encoding="utf-8") as f:
        return set(json.load(f)["vocab"].keys())


def main():
    vocab = load_kokoro_vocab()

    rows = []
    with open("dataset_tts/train_clean.csv", encoding="utf-8") as f:
        reader = csv.reader(f, delimiter="|")
        next(reader)
        for r in reader:
            rows.append((r[0], r[1]))

    backend = EspeakBackend("pt-br", preserve_punctuation=True, with_stress=True)
    texts = [t for _, t in rows]
    phonemized = backend.phonemize(texts)

    oov_counts = {}
    total_chars = 0
    cleaned = []
    for (fname, orig_text), ph in zip(rows, phonemized):
        kept = []
        for ch in ph:
            total_chars += 1
            if ch in vocab or ch == " ":
                kept.append(ch)
            else:
                oov_counts[ch] = oov_counts.get(ch, 0) + 1
        cleaned.append((fname, "".join(kept).strip()))

    oov_total = sum(oov_counts.values())
    print(f"phonemized {len(cleaned)} lines, {total_chars} chars, "
          f"{oov_total} OOV chars dropped ({oov_total / max(total_chars,1):.2%})")
    if oov_counts:
        top = sorted(oov_counts.items(), key=lambda kv: -kv[1])[:20]
        print("top OOV chars:", top)

    empty = [f for f, t in cleaned if not t]
    if empty:
        print(f"warning: {len(empty)} lines became empty after cleaning, dropping them")
        cleaned = [(f, t) for f, t in cleaned if t]

    random.seed(1)
    shuffled = cleaned[:]
    random.shuffle(shuffled)
    val = shuffled[:VAL_SIZE]
    train = shuffled[VAL_SIZE:]

    with open("dataset_tts/train_list.txt", "w", encoding="utf-8") as f:
        for fname, ph in train:
            f.write(f"{fname}|{ph}|{SPEAKER_ID}\n")

    with open("dataset_tts/val_list.txt", "w", encoding="utf-8") as f:
        for fname, ph in val:
            f.write(f"{fname}|{ph}|{SPEAKER_ID}\n")

    with open("dataset_tts/OOD_texts.txt", "w", encoding="utf-8") as f:
        for _, ph in cleaned:
            f.write(f"{ph}\n")

    print(f"train: {len(train)}  val: {len(val)}")


if __name__ == "__main__":
    main()
