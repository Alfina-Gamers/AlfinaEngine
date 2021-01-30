#ifndef AL_ENGINE_H
#define AL_ENGINE_H

#include "utilities/array_container.h"
#include "utilities/concepts.h"
#include "utilities/constexpr_functions.h"
#include "utilities/defer.h"
#include "utilities/dispensable.h"
#include "utilities/event.h"
#include "utilities/flags.h"
#include "utilities/function.h"
#include "utilities/math.h"
#include "utilities/non_copyable.h"
#include "utilities/safe_cast.h"
#include "utilities/scope_timer.h"
#include "utilities/smooth_average.h"
#include "utilities/soa.h"
#include "utilities/stack.h"
#include "utilities/static_unordered_list.h"
#include "utilities/string_processing.h"
#include "utilities/toggle.h"
#include "utilities/thread_safe/thread_safe_only_growing_stack.h"
#include "utilities/thread_safe/thread_safe_queue.h"

// @NOTE :  This file declares file system path separator for
//          target platform type. It #ifdef's different platforms
//          all by itself.
#include "engine/platform/platform_file_system_config.h"

#include "engine/config/engine_config.h"
#include "engine/containers/containers.h"
#include "engine/debug/debug.h"
#include "engine/file_system/file_load.h"
#include "engine/file_system/file_system.h"
#include "engine/game_cameras/fly_camera.h"
#include "engine/job_system/job_system.h"
#include "engine/memory/memory_common.h"
#include "engine/memory/allocator_base.h"
#include "engine/memory/dl_allocator.h"
#include "engine/memory/memory_manager.h"
#include "engine/memory/pool_allocator.h"
#include "engine/memory/stack_allocator.h"
#include "engine/memory/system_allocator.h"
#include "engine/rendering/camera/render_camera.h"
#include "engine/rendering/camera/perspective_render_camera.h"
#include "engine/rendering/geometry/geometry.h"
#include "engine/rendering/render_core.h"
#include "engine/rendering/command_buffer.h"
#include "engine/rendering/geometry_command_buffer.h"
#include "engine/rendering/framebuffer.h"
#include "engine/rendering/index_buffer.h"
#include "engine/rendering/shader.h"
#include "engine/rendering/texture_2d.h"
#include "engine/rendering/vertex_array.h"
#include "engine/rendering/vertex_buffer.h"
#include "engine/rendering/renderer.h"
#include "engine/window/os_window.h"
#include "engine/startup/alfina_engine_application.h"
#include "engine/platform/platform_thread_event.h"
#include "engine/platform/platform_thread_utilities.h"
#include "engine/platform/platform_file_system_utilities.h"
#include "engine/ecs/ecs.h"
#include "engine/scene/scene_transform.h"
#include "engine/scene/scene.h"
#ifdef _WIN32
#   include "engine/platform/win32/win32_backend.h"
#   include "engine/platform/win32/window/os_window_win32.h"
#   include "engine/platform/win32/platform_thread_event_win32.h"
#   include "engine/platform/win32/opengl/win32_opengl_backend.h"
#   include "engine/platform/win32/opengl/win32_opengl_vertex_buffer.h"
#   include "engine/platform/win32/opengl/win32_opengl_index_buffer.h"
#   include "engine/platform/win32/opengl/win32_opengl_vertex_array.h"
#   include "engine/platform/win32/opengl/win32_opengl_texture_2d.h"
#   include "engine/platform/win32/opengl/win32_opengl_shader.h"
#   include "engine/platform/win32/opengl/win32_opengl_framebuffer.h"
#   include "engine/platform/win32/opengl/win32_opengl_renderer.h"
#else
#   error Unsupported platform
#endif

// @NOTE :  Implementations
#include "engine/debug/debug.cpp"
#include "engine/file_system/file_load.cpp"
#include "engine/file_system/file_system.cpp"
#include "engine/game_cameras/fly_camera.cpp"
#include "engine/job_system/job_system.cpp"
#include "engine/memory/memory_common.cpp"
#include "engine/memory/dl_allocator.cpp"
#include "engine/memory/memory_manager.cpp"
#include "engine/memory/pool_allocator.cpp"
#include "engine/memory/stack_allocator.cpp"
#include "engine/memory/system_allocator.cpp"
#include "engine/rendering/camera/perspective_render_camera.cpp"
#include "engine/rendering/geometry/geometry.cpp"
#include "engine/rendering/framebuffer.cpp"
#include "engine/rendering/index_buffer.cpp"
#include "engine/rendering/shader.cpp"
#include "engine/rendering/texture_2d.cpp"
#include "engine/rendering/vertex_array.cpp"
#include "engine/rendering/vertex_buffer.cpp"
#include "engine/rendering/renderer.cpp"
#include "engine/window/os_window.cpp"
#include "engine/startup/alfina_engine_application.cpp"
#include "startup/entry_point.cpp"
#include "engine/ecs/ecs.cpp"
#include "engine/scene/scene_transform.cpp"
#include "engine/scene/scene.cpp"
#ifdef _WIN32
#   include "engine/platform/win32/window/os_window_win32.cpp"
#   include "engine/platform/win32/platform_thread_event_win32.cpp"
#   include "engine/platform/win32/opengl/win32_opengl_vertex_buffer.cpp"
#   include "engine/platform/win32/opengl/win32_opengl_index_buffer.cpp"
#   include "engine/platform/win32/opengl/win32_opengl_vertex_array.cpp"
#   include "engine/platform/win32/opengl/win32_opengl_texture_2d.cpp"
#   include "engine/platform/win32/opengl/win32_opengl_shader.cpp"
#   include "engine/platform/win32/opengl/win32_opengl_framebuffer.cpp"
#   include "engine/platform/win32/opengl/win32_opengl_renderer.cpp"
#   include "engine/platform/win32/platform_thread_utilities_win32.cpp"
#else
#   error Unsupported platform
#endif

#endif
