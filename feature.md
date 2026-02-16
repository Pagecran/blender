# Feature: Per-Pass Denoising for Cycles Light Passes

## Overview
This feature adds a single toggle to the Cycles Light Passes panel that automatically enables OIDN denoising for all active light passes (Diffuse, Glossy, Transmission, Volume, Emission, Background, and AO). It works independently of the Combined pass denoising toggle.

## Implementation Details

### UI (Python)
- Added `use_denoising_all_light_passes` to `CyclesRenderLayerSettings` in `properties.py`.
- Added a "Denoise Light Passes" checkbox in `ui.py` at the bottom of the Light Passes panel, aligned with existing pass headings for visual consistency.

### Cycles Sync (C++)
- Modified `sync_render_passes` in `sync.cpp` to detect the new toggle.
- When enabled, it automatically creates a `DENOISED` and a `NOISY` version for every active light pass.
- Fixed a bug where Lightgroup Combined passes were not being created correctly.

### Denoising Logic (C++)
- Added a `use_denoising` socket to the `Pass` node class.
- Updated `BufferPass` to propagate this flag to the denoiser.
- Modified `film.cpp::finalize_passes` to respect the per-pass `use_denoising` flag, preventing Cycles from stripping the denoised mode when the global combined denoising is off.
- Updated `denoiser_oidn.cpp` (CPU) and `denoiser_gpu.cpp` (GPU) to iterate over and process all passes flagged for denoising.

### Optimizations
- **Minimal Memory Overhead**: Removed the initial approach of adding `support_denoise=true` to all passes in `pass.cpp`. Instead, the `use_denoising` flag acts as an override in `film.cpp`. This ensures that memory for denoised buffers is only allocated for passes that are actually being denoised.
- **Unified Logic**: One single checkbox replaces the need for 15+ individual toggles, simplifying the UI and the synchronization logic.
