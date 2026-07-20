# App

Temporary test application for RAGE engine development. Removed when engine ships.

Goal: multiplayer raycasted voxel game.

## Shape (api-north-star)

main.cpp is being reshaped toward `.claude/plans/api-north-star.md`: one streamed
scene, zero Vulkan at the app level. The app builds the scene graph, owns the loop,
and drives `Toolkit::VoxelPipeline` — the single Vulkan-aware entry point.

- **window** — GLFW window; `vulkanSurfaceSource()` binds surface/extent callbacks
  for the pipeline so the engine never sees a window type.
- **options / world_config** — CLI parsing and the capacity-injection config
  (world sizing derives every engine limit here).
- **profiler** — the only Tracy-aware translation unit; installs the engine's
  `Core::ProfileHooks` trampoline backends.
- **demo_scene** — the game content (spinners, props, statue); a real game
  replaces this one class.
- **debug_panel / debug_ui** — the debug overlay: stats, toggles, pixel picking.
- **player_controller / free_fly_controller** — fly/walk input.
- **async_vox_loader** — worker-thread .vox fills with cancel/progress.

## App-only libraries

These libraries are used only by the app, not the engine. They will be removed alongside this folder:

- **glfw** — windowing and input
- **imgui** — debug UI
