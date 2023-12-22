#include "RAGE.hpp"
#include "RAGE_gui.hpp"
#include "RAGE_primitive_objects.hpp"

RAGE_gui::RAGE_gui(RAGE *rage)
{
	this->show_performance_window = false;
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	std::string font_path = rage->executable_path + "/assets/fonts/Roboto/Roboto-Bold.ttf";
	std::ifstream font_file(font_path.c_str());
	if (!font_file.good())
	{
		std::cout << "Failed to load font file: " << font_path << std::endl;
	}
	else
	{
		font_file.close();
		ImFont *font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), 16.0f);
	}

	io.IniFilename = NULL;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(rage->window->glfw_window, true);
	ImGui_ImplOpenGL3_Init("#version 330");
}

void RAGE_gui::draw_performance_window(RAGE *rage)
{
	ImGui::Begin("Performance window", NULL, ImGuiWindowFlags_NoResize);
	static bool first_time_open = true;
	if (first_time_open == true)
	{
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(0, 0));
		first_time_open = false;
	}
	double fps = 1000 / rage->delta_time;

	if (frames.size() > 100)
	{
		for (size_t i = 1; i < frames.size(); i++)
			frames[i - 1] = frames[i];
		frames[frames.size() - 1] = (float)fps;
	}
	else
	{
		frames.push_back((float)fps);
	}

	std::stringstream stream;
	stream << std::fixed << std::setprecision(5) << rage->delta_time;

	std::string delta_time_string = "Elapsed time: " + stream.str() + "ms";
	ImGui::Text("%s", delta_time_string.c_str());

	std::string fps_string = "FPS: " + std::to_string((int)fps);
	ImGui::Text("%s", fps_string.c_str());
	ImGui::PlotHistogram("", &frames[0], (int)frames.size(), 0, NULL, 0.0f, 360.0f, ImVec2(200, 40));
	ImGui::End();
}

void RAGE_gui::draw_inspector(RAGE *rage)
{
	ImGui::Begin("Inspector", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
	ImGui::SetWindowPos(ImVec2(0, 200));
	ImGui::SetWindowSize(ImVec2(0, 0));
	ImGui::Text("Scene object count: %zu", rage->scene.get_objects()->size());
	ImGui::End();
}

std::vector<glm::ivec2> RAGE_gui::get_window_avaliable_positions(RAGE *rage)
{
	std::vector<glm::ivec2> positions;
	glm::ivec2 window_size = rage->window->get_pixel_size();

	return (positions);
}

void RAGE_gui::draw(RAGE *rage)
{
	bool drawTriangle = true;
	float size = 1.0f;
	float color[4] = {0.8f, 0.3f, 0.02f, 1.0f};

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	scene_view.draw(rage);
	if (show_performance_window == true)
		draw_performance_window(rage);
	draw_inspector(rage);
	menu_bar.draw(rage);

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

RAGE_gui::~RAGE_gui()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}