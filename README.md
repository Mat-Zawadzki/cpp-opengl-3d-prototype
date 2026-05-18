# C++ OpenGL RTS Game Engine Showcase

A public showcase for my custom C++ / OpenGL RTS-style game engine.

The full engine source code is private. This repository contains a short demo video and selected code excerpts from the engine.

The engine is built from scratch in C++ and OpenGL, without Unity, Unreal, Godot, or another commercial game engine.

## Demo

[Watch demo video](media/demo.mp4)

The demo shows the engine running with camera movement, RTS-style building placement, placement-radius visualisation, gameplay/debug UI, and post-processing controls.

## Overview

This is an active personal game engine project focused on building lower-level real-time 3D engine systems in C++ and OpenGL.

Current work includes:

- OpenGL rendering pipeline
- GLSL shader-based rendering
- shadow pass / main scene pass / post-processing pass
- framebuffer rendering
- VBO / IBO model rendering
- instanced rendering for repeated objects
- model loading
- RAM vs VRAM asset handling
- entity and building management
- RTS-style building placement
- placement validation
- camera movement
- ImGui debug/gameplay UI

## Tech Stack

- C++
- OpenGL
- GLSL
- GLEW
- GLFW
- ImGui
- GLM
- STL
- Blender asset workflow
- Git / GitHub

## Private Project Structure

The private project is currently structured around these main systems:

```text
core/
  Game, Input, Camera, GridHandler, Save, Settings, Buttons

rendering/
  RenderPipeline, Renderer, Shader, FrameBuffer, Texture,
  VertexArray, VertexBuffer, IndexBuffer, VertexBufferLayout

entities/
  Entity, Building, Environment, EntityManager, EntityStats,
  Model, ModelManager

levels/
  BaseLevel, Level1, Level2, MainMenu
```

## Code Excerpts

The full source code is private, but this repository includes selected excerpts.

```text
code-excerpts/
  ModelExcerpt.cpp
  RenderPipelineExcerpt.cpp
```

These files are not intended to compile standalone. They are included to show implementation style and technical approach without publishing the full engine.

### `ModelExcerpt.cpp`

Shows selected parts of the model and asset-loading system, including:

- CPU-side model loading
- GPU-side upload
- VBO / IBO setup
- instance VBO handling
- model caching/management
- separation between RAM and VRAM responsibilities

### `RenderPipelineExcerpt.cpp`

Shows selected parts of the rendering pipeline, including:

- shadow pass
- main scene pass
- framebuffer rendering
- post-processing/composite pass
- placement-radius rendering
- instanced rendering path

## Main Systems

### Rendering

The renderer is built around a custom OpenGL render pipeline.

The pipeline separates rendering work into stages such as shadow rendering, main scene rendering, post-processing, compositing, and placement-preview rendering.

### Models and Assets

The model system separates loading model data into RAM from uploading OpenGL resources to VRAM.

This keeps file/asset loading separate from OpenGL resource creation and makes the system easier to manage.

### RTS Placement

The game currently includes RTS-style building placement, including placement preview, placement radius visualisation, valid/invalid placement state, and round/scoring logic.

## What This Demonstrates

This project demonstrates practical experience with:

- C++ development
- OpenGL rendering
- GLSL shaders
- framebuffers
- shadow mapping
- post-processing
- VBO / IBO workflows
- instanced rendering
- model loading
- entity management
- RTS gameplay systems
- debugging and documenting technical systems

## Note

This repository is a technical showcase, not a full source release.

The full engine remains private because it is an active personal project.
