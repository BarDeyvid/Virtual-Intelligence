// test_frame_cache.cpp
// Unit tests for FrameCache + FovealVision::analyze_cached (Phase 3.1).
// Uses synthetic cv::Mat frames — no screen capture, no model loading.

#include "../includes/VisionEnhancer.hpp"
#include <iostream>

using namespace vision_foveal;

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr)                                                     \
    do {                                                                     \
        if (!(expr)) {                                                       \
            std::cerr << "FAIL  " << name << "\n";                          \
            ++tests_failed;                                                  \
        } else {                                                             \
            ++tests_passed;                                                  \
        }                                                                    \
    } while(0)

// Frame sintético 640x480 com um retângulo branco para dar bordas ao Canny
static cv::Mat make_frame(cv::Scalar bg = cv::Scalar(40, 40, 40)) {
    cv::Mat frame(480, 640, CV_8UC3, bg);
    cv::rectangle(frame, cv::Rect(300, 220, 40, 40), cv::Scalar(255, 255, 255), cv::FILLED);
    return frame;
}

// =============================================================================
// FrameCache core behavior
// =============================================================================
static void test_cache_hits_and_misses() {
    FovealVision vision(64, 8);
    cv::Mat frame = make_frame();
    const int cx = 320, cy = 240; // cursor sobre o retângulo

    // Primeira análise: sempre miss (cache vazio)
    FovealCapture first = vision.analyze_cached(frame, cx, cy);
    TEST("first call is a miss",           vision.get_frame_cache().misses() == 1);
    TEST("first call produces tokens",     !first.color_tokens.empty());

    // Mesmo frame, mesmo cursor: hit
    FovealCapture second = vision.analyze_cached(frame, cx, cy);
    TEST("identical frame is a hit",       vision.get_frame_cache().hits() == 1);
    TEST("hit returns same tokens",        second.color_tokens == first.color_tokens);
    TEST("hit returns same edge tokens",   second.edge_tokens == first.edge_tokens);

    // Jitter de mouse dentro da célula (32px): ainda hit
    vision.analyze_cached(frame, cx + 5, cy - 3);
    TEST("cursor jitter stays cached",     vision.get_frame_cache().hits() == 2);

    // Cursor pulou pra longe: miss (crop mudou de lugar)
    vision.analyze_cached(frame, 100, 100);
    TEST("big cursor move is a miss",      vision.get_frame_cache().misses() == 2);
}

static void test_partial_change_detection() {
    FovealVision vision(64, 8);
    cv::Mat frame = make_frame();
    const int cx = 320, cy = 240;

    vision.analyze_cached(frame, cx, cy); // popula o cache

    // Mudança LONGE do crop (canto superior esquerdo): cache continua válido
    cv::Mat changed_far = frame.clone();
    cv::rectangle(changed_far, cv::Rect(0, 0, 120, 120), cv::Scalar(0, 0, 255), cv::FILLED);
    vision.analyze_cached(changed_far, cx, cy);
    TEST("change outside crop is a hit",   vision.get_frame_cache().hits() == 1);
    TEST("global change is registered",
         vision.get_frame_cache().last_global_change_ratio() > 0.0);

    // Mudança DENTRO do crop: invalida
    cv::Mat changed_near = frame.clone();
    cv::rectangle(changed_near, cv::Rect(310, 230, 20, 20), cv::Scalar(0, 255, 0), cv::FILLED);
    vision.analyze_cached(changed_near, cx, cy);
    TEST("change inside crop is a miss",   vision.get_frame_cache().misses() == 2);
}

static void test_stats_and_invalidation() {
    FovealVision vision(64, 8);
    cv::Mat frame = make_frame();

    vision.analyze_cached(frame, 320, 240); // miss
    vision.analyze_cached(frame, 320, 240); // hit
    vision.analyze_cached(frame, 320, 240); // hit

    TEST("hit rate computed",              vision.get_frame_cache().hit_rate() > 0.66 &&
                                           vision.get_frame_cache().hit_rate() < 0.67);

    vision.get_frame_cache().invalidate();
    vision.analyze_cached(frame, 320, 240);
    TEST("invalidate forces miss",         vision.get_frame_cache().misses() == 2);
}

static void test_disabled_cache() {
    FrameCache::Config cfg;
    cfg.enabled = false;
    FrameCache cache(cfg);
    cv::Mat frame = make_frame();

    FovealVision vision(64, 8);
    FovealCapture capture = vision.analyze(frame, 320, 240);
    cache.store(frame, capture);

    auto decision = cache.evaluate(frame, 320, 240);
    TEST("disabled cache never hits",      !decision.hit);
}

static void test_empty_and_edge_cases() {
    FrameCache cache;
    cv::Mat empty;
    auto decision = cache.evaluate(empty, 0, 0);
    TEST("empty frame never hits",         !decision.hit);

    // Cursor no canto: crop é clampado, mas cache ainda deve funcionar
    FovealVision vision(64, 8);
    cv::Mat frame = make_frame();
    vision.analyze_cached(frame, 0, 0);
    vision.analyze_cached(frame, 0, 0);
    TEST("corner cursor still caches",     vision.get_frame_cache().hits() == 1);
}

// =============================================================================
// latency sanity: cached path must be much cheaper than full analysis
// =============================================================================
static void test_latency_gain() {
    FovealVision vision(64, 8);
    cv::Mat frame = make_frame();

    vision.analyze_cached(frame, 320, 240); // popula

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 200; ++i) vision.analyze_cached(frame, 320, 240);
    auto cached_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();

    t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 200; ++i) vision.analyze(frame, 320, 240);
    auto full_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();

    std::cout << "[Bench] 200x cached: " << cached_us / 1000.0 << "ms | 200x full: "
              << full_us / 1000.0 << "ms\n";
    TEST("cached path is faster",          cached_us < full_us);
}

int main() {
    test_cache_hits_and_misses();
    test_partial_change_detection();
    test_stats_and_invalidation();
    test_disabled_cache();
    test_empty_and_edge_cases();
    test_latency_gain();

    std::cout << "\n========================================\n";
    std::cout << "Passed: " << tests_passed << "  Failed: " << tests_failed << "\n";
    std::cout << "========================================\n";
    return tests_failed == 0 ? 0 : 1;
}
