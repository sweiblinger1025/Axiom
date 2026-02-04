/*
 * main.cpp — Axiom Headless Runner
 *
 * TASK-000: ABI validation.
 * Proves that a C++ executable can link against AxiomCore,
 * call through the C API, and get correct results.
 *
 * This will later become the headless test/benchmark/server host.
 * For now it just validates the toolchain.
 */

#include "ax_api.h"

#include <cstdio>
#include <cstdlib>  // EXIT_SUCCESS, EXIT_FAILURE

int main()
{
    std::printf("=== Axiom Headless — ABI Validation ===\n\n");

    /* ── ABI version check ───────────────────────────────────────
     * Compare the compile-time constant (from the header we built
     * against) with the runtime value (from the loaded library).
     * Mismatch means the DLL doesn't match the header.
     */
    const uint32_t runtime_abi = ax_get_abi_version();
    const uint32_t compiled_abi = AX_ABI_VERSION;

    std::printf("ABI version (compiled): %u\n", compiled_abi);
    std::printf("ABI version (runtime):  %u\n", runtime_abi);

    if (runtime_abi != compiled_abi) {
        std::printf("\n** FAIL: ABI version mismatch! **\n");
        return EXIT_FAILURE;
    }
    std::printf("ABI version match: OK\n\n");

    /* ── Packed version ──────────────────────────────────────────
     * Decode the packed version to prove multi-field returns work.
     */
    const uint32_t version = ax_get_version_packed();
    const unsigned major = (version >> 16) & 0xFF;
    const unsigned minor = (version >>  8) & 0xFF;
    const unsigned patch =  version        & 0xFF;

    std::printf("Engine version: %u.%u.%u (packed: 0x%08X)\n",
                major, minor, patch, version);

    if (version != AX_VERSION_PACKED) {
        std::printf("\n** FAIL: packed version mismatch! **\n");
        return EXIT_FAILURE;
    }
    std::printf("Packed version match: OK\n\n");

    /* ── Build info string ───────────────────────────────────────
     * Proves const char* return across the ABI boundary.
     * Pointer is to static storage inside the library.
     */
    const char* info = ax_get_build_info();
    if (info == nullptr) {
        std::printf("** FAIL: ax_get_build_info() returned null! **\n");
        return EXIT_FAILURE;
    }
    std::printf("Build info: %s\n", info);
    std::printf("Build info return: OK\n\n");

    /* ── Allocator round-trip ────────────────────────────────────
     * Proves ax_alloc / ax_free work across the ABI boundary.
     * Critical on Windows where DLL and EXE may use different heaps.
     */
    const size_t test_size = 256;
    void* block = ax_alloc(test_size);

    if (block == nullptr) {
        std::printf("** FAIL: ax_alloc(%zu) returned null! **\n", test_size);
        return EXIT_FAILURE;
    }
    std::printf("ax_alloc(%zu): %p\n", test_size, block);

    /* Write a pattern to verify the memory is usable */
    auto* bytes = static_cast<unsigned char*>(block);
    for (size_t i = 0; i < test_size; ++i) {
        bytes[i] = static_cast<unsigned char>(i & 0xFF);
    }

    /* Verify the pattern */
    bool pattern_ok = true;
    for (size_t i = 0; i < test_size; ++i) {
        if (bytes[i] != static_cast<unsigned char>(i & 0xFF)) {
            pattern_ok = false;
            break;
        }
    }

    ax_free(block);
    block = nullptr;

    if (!pattern_ok) {
        std::printf("** FAIL: memory pattern verification failed! **\n");
        return EXIT_FAILURE;
    }
    std::printf("Allocator round-trip: OK\n\n");

    /* ── Zero-size allocation ────────────────────────────────────
     * ax_alloc(0) must return nullptr (documented behavior).
     */
    void* zero_block = ax_alloc(0);
    if (zero_block != nullptr) {
        std::printf("** FAIL: ax_alloc(0) should return nullptr! **\n");
        ax_free(zero_block);
        return EXIT_FAILURE;
    }
    std::printf("ax_alloc(0) → nullptr: OK\n\n");

    /* ── ax_free(nullptr) ────────────────────────────────────────
     * Must be a safe no-op (documented behavior).
     */
    ax_free(nullptr);
    std::printf("ax_free(nullptr): OK (no crash)\n\n");

    /* ── Summary ─────────────────────────────────────────────── */
    std::printf("=== All ABI validation checks passed ===\n");
    return EXIT_SUCCESS;
}