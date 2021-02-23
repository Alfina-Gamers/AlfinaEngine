
#include "renderer.h"

#include "engine/file_system/file_system.h"

#include "utilities/safe_cast.h"

namespace al::engine
{
    Renderer* Renderer::instance{ nullptr };

    void Renderer::allocate_space() noexcept
    {
        // @NOTE :  We (possibly in the future) will be able to switch renderer types (from OpenGL to Vulkan, for example)
        //          and this is will be done (probably) by recreating renderer instance of the new renderer type.
        //          To avoid allocating renderer memory in the pool over and over, we simply allocate space large enough to
        //          store any kind of renderer available for target system and reuse that space.
        instance = reinterpret_cast<Renderer*>(MemoryManager::get_stack()->allocate(internal::get_max_renderer_size_bytes()));
    }

    void Renderer::construct(RendererType type, OsWindow* window) noexcept
    {
        if (!instance)
        {
            return;
        }
        switch (type)
        {
            case RendererType::OPEN_GL: internal::placement_create_renderer<RendererType::OPEN_GL>(instance, window);
        }
    }

    void Renderer::destruct() noexcept
    {
        if (!instance)
        {
            return;
        }
        instance->terminate();
        instance->~Renderer();
    }

    Renderer* Renderer::get() noexcept
    {
        return instance;
    }

    Renderer::Renderer(OsWindow* window) noexcept
        : renderThread{ &Renderer::render_update, this }
        , shouldRun{ true }
        , window{ window }
        , onFrameProcessStart{ create_thread_event() }
        , onFrameProcessEnd{ create_thread_event() }
        , onCommandBufferToggled{ create_thread_event() }
        , gbuffer{ 0 }
        , gpassShader{ 0 }
        , drawFramebufferToScreenShader{ 0 }
        , screenRectangleVb{ 0 }
        , screenRectangleIb{ 0 }
        , screenRectangleVa{ 0 }
        , indexBuffers{ }
        , vertexBuffers{ }
        , vertexArrays{ }
        , shaders{ }
        , texture2ds{ }
    { }

    std::thread* Renderer::get_render_thread() noexcept
    {
        return &renderThread;
    }

    bool Renderer::is_render_thread() const noexcept
    {
        return std::this_thread::get_id() == renderThread.get_id();
    }

    void Renderer::terminate() noexcept
    {
        // @NOTE :  Can't join render in the destructor because on joining thread will call
        //          virtual terminate_renderer method, but the child will be already terminated
        shouldRun = false;
        start_process_frame();
        al_exception_wrap(renderThread.join());
    }
        
    void Renderer::start_process_frame() noexcept
    {
        al_assert(!onFrameProcessStart->is_invoked());
        onFrameProcessEnd->reset();
        onFrameProcessStart->invoke();
    }

    void Renderer::wait_for_render_finish() noexcept
    {
        al_profile_function();
        const bool waitResult = onFrameProcessEnd->wait_for(std::chrono::seconds{ 1 });
        // al_assert(waitResult); // Event was not fired. Probably something happend to render thread
    }

    void Renderer::wait_for_command_buffers_toggled() noexcept
    {
        al_profile_function();
        const bool waitResult = onCommandBufferToggled->wait_for(std::chrono::seconds{ 1 });
        // al_assert(waitResult); // Event was not fired. Probably something happend to render thread
        onCommandBufferToggled->reset();
    }

    void Renderer::set_camera(const RenderCamera* camera) noexcept
    {
        renderCamera = camera;
    }

    void Renderer::add_render_command(const RenderCommand& command) noexcept
    {
        RenderCommand* result = renderCommandBuffer.get_previous().push(command);
        al_assert(result);
    }

    [[nodiscard]] GeometryCommandData* Renderer::add_geometry_command(GeometryCommandKey key) noexcept
    {
        GeometryCommandData* result = geometryCommandBuffer.get_previous().add_command(key);
        al_assert(result);
        return result;
    }

    RendererIndexBufferHandle Renderer::reserve_index_buffer() noexcept
    {
        al_profile_function();
        IndexBuffer** ib = indexBuffers.get();
        al_assert(ib);
        *ib = nullptr;
        return
        {
            .isValid = 1,
            .index = static_cast<uint64_t>(indexBuffers.get_direct_index(ib))
        };
    }

    void Renderer::create_index_buffer(RendererIndexBufferHandle handle, uint32_t* indices, std::size_t count, IndexBufferCallback cb) noexcept
    {
        al_profile_function();
        if (is_render_thread())
        {
            index_buffer(handle) = internal::create_index_buffer<EngineConfig::DEFAULT_RENDERER_TYPE>(indices, count);
            if (cb)
            {
                const_cast<IndexBufferCallback*>(&cb)->call(handle);
            }
        }
        else
        {
            add_render_command([this, handle, indices, count, cb]()
            {
                index_buffer(handle) = internal::create_index_buffer<EngineConfig::DEFAULT_RENDERER_TYPE>(indices, count);
                if (cb)
                {
                    const_cast<IndexBufferCallback*>(&cb)->call(handle);
                }
            });
        }
    }

    void Renderer::destroy_index_buffer(RendererIndexBufferHandle handle) noexcept
    {
        al_profile_function();
        if (is_render_thread())
        {
            internal::destroy_index_buffer<EngineConfig::DEFAULT_RENDERER_TYPE>(index_buffer(handle));
            index_buffer(handle) = nullptr;
        }
        else
        {
            add_render_command([this, handle]()
            {
                internal::destroy_index_buffer<EngineConfig::DEFAULT_RENDERER_TYPE>(index_buffer(handle));
                index_buffer(handle) = nullptr;
            });
        }
    }

    inline IndexBuffer*& Renderer::index_buffer(RendererIndexBufferHandle handle) noexcept
    {
        al_assert(handle.value != 0);
        return *indexBuffers.direct_accsess(static_cast<std::size_t>(handle.index));
    }

    RendererVertexBufferHandle Renderer::reserve_vertex_buffer() noexcept
    {
        al_profile_function();
        VertexBuffer** vb = vertexBuffers.get();
        al_assert(vb);
        *vb = nullptr;
        return
        {
            .isValid = 1,
            .index = static_cast<uint64_t>(vertexBuffers.get_direct_index(vb))
        };
    }

    void Renderer::create_vertex_buffer(RendererVertexBufferHandle handle, const void* data, std::size_t size, VertexBufferCallback cb) noexcept
    {
        al_profile_function();
        if (is_render_thread())
        {
            vertex_buffer(handle) = internal::create_vertex_buffer<EngineConfig::DEFAULT_RENDERER_TYPE>(data, size);
            if (cb)
            {
                const_cast<VertexBufferCallback*>(&cb)->call(handle);
            }
        }
        else
        {
            add_render_command([this, handle, data, size, cb]()
            {
                vertex_buffer(handle) = internal::create_vertex_buffer<EngineConfig::DEFAULT_RENDERER_TYPE>(data, size);
                if (cb)
                {
                    const_cast<VertexBufferCallback*>(&cb)->call(handle);
                }
            });
        }
    }

    void Renderer::destroy_vertex_buffer(RendererVertexBufferHandle handle) noexcept
    {
        al_profile_function();
        if (is_render_thread())
        {
            internal::destroy_vertex_buffer<EngineConfig::DEFAULT_RENDERER_TYPE>(vertex_buffer(handle));
            vertex_buffer(handle) = nullptr;
        }
        else
        {
            add_render_command([this, handle]()
            {
                internal::destroy_vertex_buffer<EngineConfig::DEFAULT_RENDERER_TYPE>(vertex_buffer(handle));
                vertex_buffer(handle) = nullptr;
            });
        }
    }

    inline VertexBuffer*& Renderer::vertex_buffer(RendererVertexBufferHandle handle) noexcept
    {
        al_assert(handle.value != 0);
        return *vertexBuffers.direct_accsess(static_cast<std::size_t>(handle.index));
    }

    RendererVertexArrayHandle Renderer::reserve_vertex_array() noexcept
    {
        al_profile_function();
        VertexArray** va = vertexArrays.get();
        al_assert(va);
        *va = nullptr;
        return
        {
            .isValid = 1,
            .index = static_cast<uint64_t>(vertexArrays.get_direct_index(va))
        };
    }

    void Renderer::create_vertex_array(RendererVertexArrayHandle handle, VertexArrayCallback cb) noexcept
    {
        al_profile_function();
        if (is_render_thread())
        {
            vertex_array(handle) = internal::create_vertex_array<EngineConfig::DEFAULT_RENDERER_TYPE>();
            if (cb)
            {
                const_cast<VertexArrayCallback*>(&cb)->call(handle);
            }
        }
        else
        {
            add_render_command([this, handle, cb]()
            {
                vertex_array(handle) = internal::create_vertex_array<EngineConfig::DEFAULT_RENDERER_TYPE>();
                if (cb)
                {
                    const_cast<VertexArrayCallback*>(&cb)->call(handle);
                }
            });
        }
    }

    void Renderer::destroy_vertex_array(RendererVertexArrayHandle handle) noexcept
    {
        al_profile_function();
        if (is_render_thread())
        {
            internal::destroy_vertex_array<EngineConfig::DEFAULT_RENDERER_TYPE>(vertex_array(handle));
            vertex_array(handle) = nullptr;
        }
        else
        {
            add_render_command([this, handle]()
            {
                internal::destroy_vertex_array<EngineConfig::DEFAULT_RENDERER_TYPE>(vertex_array(handle));
                vertex_array(handle) = nullptr;
            });
        }
    }

    inline VertexArray*& Renderer::vertex_array(RendererVertexArrayHandle handle) noexcept
    {
        al_assert(handle.value != 0);
        return *vertexArrays.direct_accsess(static_cast<std::size_t>(handle.index));
    }

    RendererShaderHandle Renderer::reserve_shader() noexcept
    {
        al_profile_function();
        Shader** s = shaders.get();
        al_assert(s);
        *s = nullptr;
        return
        {
            .isValid = 1,
            .index = static_cast<uint64_t>(shaders.get_direct_index(s))
        };
    }

    void Renderer::create_shader(RendererShaderHandle handle, std::string_view vertexShaderSrc, std::string_view fragmentShaderSrc, ShaderCallback cb) noexcept
    {
        al_profile_function();
        if (is_render_thread())
        {
            shader(handle) = internal::create_shader<EngineConfig::DEFAULT_RENDERER_TYPE>(vertexShaderSrc, fragmentShaderSrc);
            if (cb)
            {
                const_cast<ShaderCallback*>(&cb)->call(handle);
            }
        }
        else
        {
            add_render_command([this, handle, vertexShaderSrc, fragmentShaderSrc, cb]()
            {
                shader(handle) = internal::create_shader<EngineConfig::DEFAULT_RENDERER_TYPE>(vertexShaderSrc, fragmentShaderSrc);
                if (cb)
                {
                    const_cast<ShaderCallback*>(&cb)->call(handle);
                }
            });
        }
    }

    void Renderer::destroy_shader(RendererShaderHandle handle) noexcept
    {
        al_profile_function();
        if (is_render_thread())
        {
            internal::destroy_shader<EngineConfig::DEFAULT_RENDERER_TYPE>(shader(handle));
            shader(handle) = nullptr;
        }
        else
        {
            add_render_command([this, handle]()
            {
                internal::destroy_shader<EngineConfig::DEFAULT_RENDERER_TYPE>(shader(handle));
                shader(handle) = nullptr;
            });
        }
    }

    inline Shader*& Renderer::shader(RendererShaderHandle handle) noexcept
    {
        al_assert(handle.value != 0);
        return *shaders.direct_accsess(static_cast<std::size_t>(handle.index));
    }

    RendererFramebufferHandle Renderer::reserve_framebuffer() noexcept
    {
        al_profile_function();
        Framebuffer** f = framebuffers.get();
        al_assert(f);
        *f = nullptr;
        return
        {
            .isValid = 1,
            .index = static_cast<uint64_t>(framebuffers.get_direct_index(f))
        };
    }

    void Renderer::create_framebuffer(RendererFramebufferHandle handle, const FramebufferDescription& description, FramebufferCallback cb) noexcept
    {
        al_profile_function();
        if (is_render_thread())
        {
            framebuffer(handle) = internal::create_framebuffer<EngineConfig::DEFAULT_RENDERER_TYPE>(description);
            if (cb)
            {
                const_cast<FramebufferCallback*>(&cb)->call(handle);
            }
        }
        else
        {
            add_render_command([this, handle, description, cb]()
            {
                framebuffer(handle) = internal::create_framebuffer<EngineConfig::DEFAULT_RENDERER_TYPE>(description);
                if (cb)
                {
                    const_cast<FramebufferCallback*>(&cb)->call(handle);
                }
            });
        }
    }

    void Renderer::destroy_framebuffer(RendererFramebufferHandle handle) noexcept
    {
        al_profile_function();
        if (is_render_thread())
        {
            internal::destroy_framebuffer<EngineConfig::DEFAULT_RENDERER_TYPE>(framebuffer(handle));
            framebuffer(handle) = nullptr;
        }
        else
        {
            add_render_command([this, handle]()
            {
                internal::destroy_framebuffer<EngineConfig::DEFAULT_RENDERER_TYPE>(framebuffer(handle));
                framebuffer(handle) = nullptr;
            });
        }
    }

    inline Framebuffer*& Renderer::framebuffer(RendererFramebufferHandle handle) noexcept
    {
        al_assert(handle.value != 0);
        return *framebuffers.direct_accsess(static_cast<std::size_t>(handle.index));
    }

    RendererTexture2dHandle Renderer::reserve_texture_2d() noexcept
    {
        al_profile_function();
        Texture2d** tex = texture2ds.get();
        al_assert(tex);
        *tex = nullptr;
        return
        {
            .isValid = 1,
            .index = static_cast<uint64_t>(texture2ds.get_direct_index(tex))
        };
    }

    void Renderer::create_texture_2d(RendererTexture2dHandle handle, std::string_view path, Texture2dCallback cb) noexcept
    {
        al_profile_function();
        if (is_render_thread())
        {
            texture_2d(handle) = internal::create_texture_2d<EngineConfig::DEFAULT_RENDERER_TYPE>(path);
            if (cb)
            {
                const_cast<Texture2dCallback*>(&cb)->call(handle);
            }
        }
        else
        {
            add_render_command([this, handle, path, cb]()
            {
                texture_2d(handle) = internal::create_texture_2d<EngineConfig::DEFAULT_RENDERER_TYPE>(path);
                if (cb)
                {
                    const_cast<Texture2dCallback*>(&cb)->call(handle);
                }
            });
        }
    }

    void Renderer::destroy_texture_2d(RendererTexture2dHandle handle) noexcept
    {
        al_profile_function();
        if (is_render_thread())
        {
            internal::destroy_texture_2d<EngineConfig::DEFAULT_RENDERER_TYPE>(texture_2d(handle));
            texture_2d(handle) = nullptr;
        }
        else
        {
            add_render_command([this, handle]()
            {
                internal::destroy_texture_2d<EngineConfig::DEFAULT_RENDERER_TYPE>(texture_2d(handle));
                texture_2d(handle) = nullptr;
            });
        }
    }

    inline Texture2d*& Renderer::texture_2d(RendererTexture2dHandle handle) noexcept
    {
        al_assert(handle.value != 0);
        return *texture2ds.direct_accsess(static_cast<std::size_t>(handle.index));
    }

    void Renderer::render_update() noexcept
    {
        initialize_renderer();
        {
            al_profile_scope("Renderer post-init");
            {
                FramebufferDescription gbufferDesciption;
                gbufferDesciption.attachments =
                {
                    FramebufferAttachmentType::RGB_8,               // Position
                    FramebufferAttachmentType::RGB_8,               // Normal
                    FramebufferAttachmentType::RGB_8,               // Albedo
                    FramebufferAttachmentType::DEPTH_24_STENCIL_8   // Depth + stencil (maybe?)
                };
                gbufferDesciption.width = window->get_params()->width;
                gbufferDesciption.height = window->get_params()->height;
                gbuffer = reserve_framebuffer();
                create_framebuffer(gbuffer, gbufferDesciption);
            }
            {
                FileHandle* vertGpassShaderSrc = FileSystem::get()->sync_load(EngineConfig::DEFFERED_GEOMETRY_PASS_VERT_SHADER_PATH, FileLoadMode::READ);
                FileHandle* fragGpassShaderSrc = FileSystem::get()->sync_load(EngineConfig::DEFFERED_GEOMETRY_PASS_FRAG_SHADER_PATH, FileLoadMode::READ);
                const char* vertGpassShaderStr = reinterpret_cast<const char*>(vertGpassShaderSrc->memory);
                const char* fragGpassShaderStr = reinterpret_cast<const char*>(fragGpassShaderSrc->memory);
                gpassShader = reserve_shader();
                create_shader(gpassShader, vertGpassShaderStr, fragGpassShaderStr, [this, vertGpassShaderSrc, fragGpassShaderSrc](RendererShaderHandle handle)
                {
                    FileSystem::get()->free_handle(vertGpassShaderSrc);
                    FileSystem::get()->free_handle(fragGpassShaderSrc);
                    shader(handle)->bind();
                    shader(handle)->set_int(EngineConfig::DEFFERED_GEOMETRY_PASS_DIFFUSE_TEXTURE_NAME, EngineConfig::DEFFERED_GEOMETRY_PASS_DIFFUSE_TEXTURE_LOCATION);
                });
            }
            {
                FileHandle* vertDrawFramebufferToScreenShaderSrc = FileSystem::get()->sync_load(EngineConfig::DRAW_FRAMEBUFFER_TO_SCREEN_VERT_SHADER_PATH, FileLoadMode::READ);
                FileHandle* fragDrawFramebufferToScreenShaderSrc = FileSystem::get()->sync_load(EngineConfig::DRAW_FRAMEBUFFER_TO_SCREEN_FRAG_SHADER_PATH, FileLoadMode::READ);
                const char* vertDrawFramebufferToScreenShaderStr = reinterpret_cast<const char*>(vertDrawFramebufferToScreenShaderSrc->memory);
                const char* fragDrawFramebufferToScreenShaderStr = reinterpret_cast<const char*>(fragDrawFramebufferToScreenShaderSrc->memory);
                drawFramebufferToScreenShader = reserve_shader();
                create_shader(drawFramebufferToScreenShader, vertDrawFramebufferToScreenShaderStr, fragDrawFramebufferToScreenShaderStr, 
                [vertDrawFramebufferToScreenShaderSrc, fragDrawFramebufferToScreenShaderSrc](RendererShaderHandle handle)
                {
                    FileSystem::get()->free_handle(vertDrawFramebufferToScreenShaderSrc);
                    FileSystem::get()->free_handle(fragDrawFramebufferToScreenShaderSrc);
                });
            }
            {
                static float screenPlaneVertices[] =
                {
                    -1.0f, -1.0f, 0.0f, 0.0f,   // Bottom left
                     1.0f, -1.0f, 1.0f, 0.0f,   // Bottom right
                    -1.0f,  1.0f, 0.0f, 1.0f,   // Top left
                     1.0f,  1.0f, 1.0f, 1.0f    // Top right
                };
                static uint32_t screenPlaneIndices[] = 
                {
                    0, 1, 2,
                    2, 1, 3
                };
                screenRectangleVb = reserve_vertex_buffer();
                create_vertex_buffer(screenRectangleVb, screenPlaneVertices, sizeof(screenPlaneVertices), [this](RendererVertexBufferHandle handle)
                {
                    vertex_buffer(handle)->set_layout(BufferLayout::ElementContainer{
                        BufferElement{ ShaderDataType::Float2, false }, // Position
                        BufferElement{ ShaderDataType::Float2, false }  // Uv
                    });
                });
                screenRectangleIb = reserve_index_buffer();
                create_index_buffer(screenRectangleIb, screenPlaneIndices, sizeof(screenPlaneIndices) / sizeof(uint32_t));
                screenRectangleVa = reserve_vertex_array();
                create_vertex_array(screenRectangleVa, [this](RendererVertexArrayHandle handle)
                {
                    vertex_array(handle)->set_vertex_buffer(vertex_buffer(screenRectangleVb));
                    vertex_array(handle)->set_index_buffer(index_buffer(screenRectangleIb));
                });
            }
            // Enable vsync
            set_vsync_state(true);
        }
        while (true)
        {
            wait_for_render_start();
            if (!shouldRun) { break; }
            {
                al_profile_scope("Render Frame");
                // @NOTE : Don't know where is the best place for buffers toggle
                {
                    al_profile_scope("Toggle command buffers");
                    renderCommandBuffer.toggle();
                    geometryCommandBuffer.toggle();
                    notify_command_buffers_toggled();
                }
                {
                    al_profile_scope("Process render commands");
                    RenderCommandBuffer& current = renderCommandBuffer.get_current();
                    current.for_each([](RenderCommand* command)
                    {
                        (*command)();
                    });
                    current.clear();
                }
                {
                    al_profile_scope("Geometry pass");
                    // Bind buffer and shader
                    framebuffer(gbuffer)->bind();
                    shader(gpassShader)->bind();
                    clear_buffers();
                    // Set view-projection matrix
                    const float4x4 vpMatrix = (renderCamera->get_projection() * renderCamera->get_view()).transposed();
                    shader(gpassShader)->set_mat4(EngineConfig::SHADER_VIEW_PROJECTION_MATRIX_UNIFORM_NAME, vpMatrix);
                    // Draw geometry
                    GeometryCommandBuffer& current = geometryCommandBuffer.get_current();
                    current.sort();
                    current.for_each([this](GeometryCommandData* data)
                    {
                        if (!data->va)
                        {
                            al_log_error(EngineConfig::RENDERER_LOG_CATEGORY, "Trying to process draw command, but vertex array is null");
                            return;
                        }
                        if (!data->diffuseTexture)
                        {
                            al_log_error(EngineConfig::RENDERER_LOG_CATEGORY, "Trying to process draw command, but diffuse texture is null");
                            return;
                        }
                        data->diffuseTexture->bind(EngineConfig::DEFFERED_GEOMETRY_PASS_DIFFUSE_TEXTURE_LOCATION);
                        shader(gpassShader)->set_mat4(EngineConfig::SHADER_MODEL_MATRIX_UNIFORM_NAME, data->trf.matrix.transposed());
                        draw(data->va);
                    });
                    current.clear();
                }
                {
                    al_profile_scope("Draw to screen pass");
                    // Bind buffer and shader
                    bind_screen_framebuffer();
                    shader(drawFramebufferToScreenShader)->bind();
                    clear_buffers(); // Probably this is not needed
                    // Attach texture to be drawn on screen
                    framebuffer(gbuffer)->bind_attachment_to_slot(2, EngineConfig::SCREEN_PASS_SOURCE_BUFFER_TEXTURE_LOCATION);
                    shader(drawFramebufferToScreenShader)->set_int(EngineConfig::SCREEN_PASS_SOURCE_BUFFER_TEXTURE_NAME, EngineConfig::SCREEN_PASS_SOURCE_BUFFER_TEXTURE_LOCATION);
                    // Draw screen quad with selected texture
                    set_depth_test_state(false);
                    draw(vertex_array(screenRectangleVa));
                    set_depth_test_state(true);
                }
                {
                    al_profile_scope("Swap render buffers");
                    swap_buffers();
                }
            }
            notify_render_finished();
        }
        {
            al_profile_scope("Renderer pre-terminate");
            destroy_framebuffer(gbuffer);
            destroy_shader(gpassShader);
            destroy_shader(drawFramebufferToScreenShader);
            destroy_vertex_array(screenRectangleVa);
            destroy_vertex_buffer(screenRectangleVb);
            destroy_index_buffer(screenRectangleIb);
        }
        terminate_renderer();
    }

    void Renderer::wait_for_render_start() noexcept
    {
        al_profile_function();
        onFrameProcessStart->wait();
    }

    void Renderer::notify_render_finished() noexcept
    {
        al_assert(!onFrameProcessEnd->is_invoked());
        onFrameProcessStart->reset();
        onFrameProcessEnd->invoke();
    }

    void Renderer::notify_command_buffers_toggled() noexcept
    {
        al_assert(!onCommandBufferToggled->is_invoked());
        onCommandBufferToggled->invoke();
    }

    namespace internal
    {
        std::size_t internal::get_max_renderer_size_bytes() noexcept
        {
            std::size_t result = 0;
            {
                std::size_t size = internal::get_renderer_size_bytes<RendererType::OPEN_GL>();
                result = size > result ? size : result;
            }
            return result;
        }
    }
}
