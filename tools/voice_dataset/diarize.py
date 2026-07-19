"""
Clusters a folder of short audio clips by speaker so a single-speaker subset
can be picked out before Kokoro fine-tuning.

Usage:
    python diarize.py --input ../../audios_teste --output ../../audios_teste_clusters
    python diarize.py --input ../../audios_teste --output ../../audios_teste_clusters --k 3

Without --k, tries k=1..6 and reports silhouette scores so you can judge how
many distinct voices are actually present (the dubbers may sound close enough
to land in one cluster).
"""

import argparse
import json
import shutil
from pathlib import Path

import numpy as np
from resemblyzer import VoiceEncoder, preprocess_wav
from sklearn.cluster import AgglomerativeClustering
from sklearn.metrics import silhouette_score


def embed_all(input_dir: Path, encoder: VoiceEncoder):
    paths = sorted(input_dir.glob("*.mp3")) + sorted(input_dir.glob("*.wav"))
    embeddings = []
    kept_paths = []
    for i, path in enumerate(paths):
        try:
            wav = preprocess_wav(path)
            embeddings.append(encoder.embed_utterance(wav))
            kept_paths.append(path)
        except Exception as e:
            print(f"  skip {path.name}: {e}")
        if (i + 1) % 100 == 0:
            print(f"  embedded {i + 1}/{len(paths)}")
    return kept_paths, np.stack(embeddings)


def best_k(embeddings: np.ndarray, k_range: range):
    scores = {}
    for k in k_range:
        labels = AgglomerativeClustering(n_clusters=k, metric="cosine", linkage="average").fit_predict(embeddings)
        if k == 1:
            continue
        scores[k] = silhouette_score(embeddings, labels, metric="cosine")
    return scores


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True, type=Path)
    ap.add_argument("--output", required=True, type=Path)
    ap.add_argument("--k", type=int, default=None, help="force a cluster count instead of auto-scoring")
    ap.add_argument("--k-max", type=int, default=6, help="max k to try when auto-scoring")
    ap.add_argument("--no-copy", action="store_true", help="only write the manifest, skip copying audio into cluster folders")
    args = ap.parse_args()

    print("loading encoder...")
    encoder = VoiceEncoder()

    print(f"embedding clips from {args.input}...")
    paths, embeddings = embed_all(args.input, encoder)
    print(f"embedded {len(paths)} clips")

    if args.k is None:
        scores = best_k(embeddings, range(2, args.k_max + 1))
        print("\nsilhouette score by k (higher = cleaner separation, <0.1 usually means 'not really separable'):")
        for k, s in scores.items():
            print(f"  k={k}: {s:.3f}")
        k = max(scores, key=scores.get)
        print(f"\nauto-picked k={k}; rerun with --k N to override")
    else:
        k = args.k

    labels = AgglomerativeClustering(n_clusters=k, metric="cosine", linkage="average").fit_predict(embeddings)

    args.output.mkdir(parents=True, exist_ok=True)
    manifest = {}
    for path, label in zip(paths, labels):
        manifest[path.name] = int(label)
        if not args.no_copy:
            cluster_dir = args.output / f"cluster_{label}"
            cluster_dir.mkdir(exist_ok=True)
            shutil.copy2(path, cluster_dir / path.name)

    manifest_path = args.output / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")

    counts = {}
    for label in labels:
        counts[int(label)] = counts.get(int(label), 0) + 1
    print(f"\ncluster sizes: {counts}")
    print(f"manifest written to {manifest_path}")
    if not args.no_copy:
        print(f"clips copied under {args.output}/cluster_*/ for you to listen through and pick the right one(s)")


if __name__ == "__main__":
    main()
