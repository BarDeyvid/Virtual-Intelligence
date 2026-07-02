# Alyssa Refactor — 2-Week Plan

Guiding rule: this sprint is for **deciding and cutting**, not adding. If a task
tempts you to build something new, park it in "Ideas for later" at the bottom
and keep moving.

---

## Phase 1 — Kill the forks, pick one truth

- [x] Confirm `Alyssa_CLI_2.cpp` is the real entry point; rename it `main.cpp`
      (or keep the name if you prefer, just be deliberate about it).
- [x] Move `Alyssa_TRUE.cpp`, `Alyssa_CLI.cpp`, `Alyssa_API.cpp` into a
      `legacy/` folder. Don't delete yet — you may want to scavenge a line
      or two — but they stop compiling as part of the main target.
- [x] Port the Top-K gating logic from `think_with_fusion_optimized` into
      `think_with_fusion_ttsless` (see Phase 2 below — this is the one real
      behavior change in this refactor, everything else is cleanup).
- [x] Delete `think_with_fusion_optimized` and plain `think()` once their
      logic is absorbed or confirmed unused.
- [x] Turn `think_with_fusion` (TTS version) into a thin wrapper over the
      same core logic as `think_with_fusion_ttsless`, parameterized by a
      `bool use_tts` + `ElevenLabsTTS*`, instead of a duplicated function
      body. Right now a bug fix has to be made in two places by hand.

## Phase 2 — Fix the fusion so it's actually weighted

- [x] In `think_with_fusion_ttsless`, replace the "run all 11 experts always"
      loop with the rule-based gating + Top-K selection from
      `think_with_fusion_optimized`.
- [x] Set `contrib.weight` on each `ExpertContribution` from the gating
      weights (currently unused in the live path).
- [x] In `generate_fused_input`, sort contributions by `weight` descending
      before building the `[PENSAMENTOS]` block, so the most relevant
      thought lands first in the prompt.
- [x] Decide `top_k` and `threshold` empirically — start with `top_k=3`,
      `threshold=0.15` (values already in your codebase) and adjust based
      on whether responses feel thinner or better.
- [x] Delete the ONNX router path entirely: `calculate_neural_weights`,
      `calculate_feature_based_weights`, the `Ort::Session`/`Ort::Env`
      members in `WeightedFusion`, and `fuse_responses` (currently unused).
- [x] Remove `fusion_router.onnx` / `fusion_router.onnx.data` from the repo
      root once the code referencing them is gone.
- [ ] Optional, not required: upgrade `calculate_rule_based_weights` to also
      use cosine similarity between the input embedding and a short
      one-sentence "profile" per persona — cheap, keeps the `Embedder`
      you're already running, and needs no training data or ONNX model.

## Phase 3 — Naming and file structure

- [x] Confirm `AlyssaCore.hpp` stays standalone (model + context wrapper,
      used by every expert).
- [x] Sweep for other header/impl name mismatches:
      - Created `includes/pc_metrics_reader.hpp` (was `.cpp` included as header)
      - Fixed `ExpertBase.hpp` to `#include "pc_metrics_reader.hpp"`
      - Deleted dead `run_expert_committee` (replaced by inline in core)
      - Removed all ONNX references (`calculate_neural_weights`, `fuse_responses`, etc.)
      - Removed stale `think_with_fusion_optimized` and `think()` declarations
- [x] Remove stale identifiers: completed — leftover fusion-strategy references
      (`fuse_responses`, `calculate_neural_weights`, etc.) are all gone.

## Phase 4 — Real bugs worth fixing while you're in there

- [x] `manage_dynamic_history` memory safety:
      - Added helpers `push_chat_message` / `free_chat_history` in
        `AlyssaCore.hpp` replacing all raw `strdup`+`push_back` and
        `free((char*)msg.content)` throughout `ExpertBase` and
        `CoreIntegration` clean-up paths.
- [x] Thread safety in `Alyssa_CLI.cpp`:
      - Added `std::mutex brain_mtx` to `AppState`
      - `on_enter` now wraps `think_with_fusion_ttsless` call in
        `std::lock_guard<std::mutex> brain_lock(g_state.brain_mtx)`
- [x] `const_cast` in `fuse_responses` is dead — function was deleted in
      Phase 2.

## Phase 5 — Cheap wins for confidence

- [x] Write tests for the pure, model-free functions only:
      - Created `includes/FusionUtils.hpp` with free-function versions:
        `is_small_talk`, `calculate_string_similarity`,
        `calculate_committee_coherence`, `calculate_history_limit`,
        `apply_topk_gating`
      - Created `tests/test_fusion_utils.cpp` (~160 lines) exercising all
        functions with edge cases
      - Added `test_fusion_utils` target to `CMakeLists.txt`
      - **No inference dependencies** — compiles against the STL only.

---

## Ideas for later (explicitly not this sprint)

- Real semantic router to replace/augment rule-based gating (needs data
  and time you don't have in 2 weeks).
- Splitting `AlyssaCore.hpp`'s `generate_raw` into header + `.cpp`.
- Revisiting whether all 11 personas need to exist, vs. consolidating
  overlapping ones (e.g. `zenModel` vs `peacekeeperModel` — decide later
  whether they're pulling meaningfully different weight).
