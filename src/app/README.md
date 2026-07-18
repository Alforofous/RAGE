# App

Temporary test application for RAGE engine development. Removed when engine ships.

Goal: multiplayer raycasted voxel game.

## Shape (api-north-star)

main.cpp is being reshaped toward `.claude/plans/api-north-star.md`: one streamed
scene, zero Vulkan at the app level. The app builds the scene graph, owns the loop,
and drives `Toolkit::VoxelPipeline` — the single Vulkan-aware entry point.

- **window** — GLFW window; `vulkanSurfaceSource()` binds surface/extent callbacks
  for the pipeline so the engine never sees a window type.
- **profiler** — the only Tracy-aware translation unit; installs the engine's
  `Core::ProfileHooks` trampoline backends.
- **debug_ui** — ImGui overlay (stats, heatmap/SVDAG toggles).
- **free_fly_controller** — mouse-look + fly movement; vetoed by walk mode.

## App-only libraries

These libraries are used only by the app, not the engine. They will be removed alongside this folder:

- **glfw** — windowing and input
- **imgui** — debug UI
