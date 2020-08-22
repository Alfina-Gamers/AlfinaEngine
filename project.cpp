#include "project.h"

int main()
{
	AL_LOG(al::engine::Logger::WARNING, "Test warning ", 1, 2.0f, 3.1234, al::float2{ 2, 3 })

	al::engine::ApplicationWindow* window;
	al::engine::WindowProperties properties
	{
		al::nonSpecifiedValue,
		al::nonSpecifiedValue,
		al::nonSpecifiedValue,
		al::nonSpecifiedValue,
		al::engine::WindowProperties::ScreenMode::WINDOWED,
		"Application window"
	};
	al::engine::create_application_window(properties, &window);

	// setup vertex buffer
	float arr[] =
	{
		-1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
		1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
		-1.0f,  1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f
	};

	al::engine::VertexBuffer* vb;
	al::engine::create_vertex_buffer(&vb, arr, sizeof(arr));
	vb->set_layout
	({
		{ al::engine::ShaderDataType::Float3 },
		{ al::engine::ShaderDataType::Float4 }
	});

	// setup index buffer
	uint32_t ids[] = { 0, 1, 2 };
	al::engine::IndexBuffer* ib;
	al::engine::create_index_buffer(&ib, ids, sizeof(ids));

	// setup vertex array
	al::engine::VertexArray* va;
	al::engine::create_vertex_array(&va);
	va->set_vertex_buffer(vb);
	va->set_index_buffer(ib);

	al::engine::FileHandle vertexShader;
	al::engine::FileSys::read_file("Shaders\\vertex.vert", &vertexShader);

	al::engine::FileHandle fragmentShader;
	al::engine::FileSys::read_file("Shaders\\fragment.frag", &fragmentShader);

	al::engine::Shader* shader;
	al::engine::create_shader(&shader, (const char*)vertexShader.get_data(), (const char*)fragmentShader.get_data());

	while (!window->input.generalInput.get_flag(al::engine::ApplicationWindowInput::GeneralInputFlags::CLOSE_BUTTON_PRESSED))
	{
		static al::float4 tint{ 1.0f, 1.0f, 1.0f, 1.0f };
		tint[0] = tint[0] + 0.01f; if (tint[0] > 1.0f) tint[0] -= 1.0f;
		tint[1] = tint[1] + 0.01f; if (tint[1] > 1.0f) tint[1] -= 1.0f;
		tint[2] = tint[2] + 0.01f; if (tint[2] > 1.0f) tint[2] -= 1.0f;

		window->renderer->make_current();

		window->renderer->clear_screen({ 0.1f, 0.1f, 0.1f });
		shader->set_float4("tint", tint);
		window->renderer->draw(shader, va);

		window->renderer->commit();
	}

	al::engine::destroy_vertex_buffer(vb);
	al::engine::destroy_index_buffer(ib);
	al::engine::destroy_vertex_array(va);

	al::engine::destroy_shader(shader);

	al::engine::destroy_application_window(window);

    return 0;
}