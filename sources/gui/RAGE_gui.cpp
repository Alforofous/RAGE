#include "RAGE.hpp"
#include "gui/RAGE_gui.hpp"
#include "RAGE_primitive_objects.hpp"

static bool load_imgui_font(RAGE *rage, ImGuiIO *io)
{
	std::string font_path = rage->executable_path + "/assets/fonts/Roboto/Roboto-Bold.ttf";
	std::ifstream font_file(font_path.c_str());
	if (!font_file.good())
	{
		std::cout << "Failed to load font file: " << font_path << std::endl;
		return (false);
	}
	font_file.close();
	ImFont *font = io->Fonts->AddFontFromFileTTF(font_path.c_str(), 16.0f);
	return (true);
}

RAGE_gui::RAGE_gui(RAGE *rage)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	this->show_performance_window = false;
	this->io = &ImGui::GetIO();
	io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	load_imgui_font(rage, this->io);
	io->IniFilename = NULL;

	io->UserData = (void *)rage;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(rage->window->glfw_window, true);
	ImGui_ImplGlfw_SetCallbacksChainForAllWindows(true);
	ImGui_ImplOpenGL3_Init("#version 330");
}

ImGuiIO *RAGE_gui::get_imgui_io()
{
	return (this->io);
}

void RAGE_gui::draw_performance_window(RAGE *rage)
{
	static double time_passed; 
	static double delta_time;
	static double fps;

	time_passed += rage->delta_time;
	if (time_passed > 1000 / 30.0)
	{
		fps = 1000 / rage->delta_time;
		delta_time = rage->delta_time;
		time_passed = 0.0;
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
	}

	ImGui::Begin("Performance", NULL, ImGuiWindowFlags_NoResize);

	std::stringstream stream;
	stream << std::fixed << std::setprecision(5) << delta_time;

	std::string delta_time_string = "Elapsed time: " + stream.str() + "ms";
	ImGui::Text("%s", delta_time_string.c_str());

	std::string fps_string = "FPS: " + std::to_string((int)fps);
	ImGui::Text("%s", fps_string.c_str());
	ImGui::PlotHistogram("", &frames[0], (int)frames.size(), 0, NULL, 0.0f, 2000.0f, ImVec2(200, 40));
	std::string frames_string;
	for (size_t i = 0; i < frames.size(); i++)
	{
		frames_string += std::to_string((int)frames[i]);
		if (i != frames.size() - 1)
			frames_string += ", ";
	}
	ImGui::End();
}

void RAGE_gui::draw_dockspace(RAGE *rage)
{
	this->viewport = ImGui::GetMainViewport();
	this->dockspace_id = ImGui::DockSpaceOverViewport(this->viewport, ImGuiDockNodeFlags_None);
	this->dockspace_size = glm::vec2(this->viewport->WorkSize.x, this->viewport->WorkSize.y);
}

void RAGE_gui::reset_dockings()
{
	ImGui::DockBuilderRemoveNode(dockspace_id);
	ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspace_id, this->viewport->WorkSize);

	ImGuiID dock_main_id = dockspace_id;
	ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, NULL, &dock_main_id);
	ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.30f, NULL, &dock_main_id);

	ImGui::DockBuilderDockWindow("Scene View", dock_main_id);
	ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
	ImGui::DockBuilderDockWindow("Performance", dock_id_left);

	ImGui::DockBuilderFinish(dockspace_id);
}

void RAGE_gui::draw(RAGE *rage)
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	draw_dockspace(rage);
	if (show_performance_window == true)
		draw_performance_window(rage);
	scene_view.draw(rage);
	menu_bar.draw(rage);
	inspector.draw(rage);

	static bool first_time = true;
	if (first_time == true)
	{
		reset_dockings();
		first_time = false;
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	ImGui::EndFrame();
	if (this->io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		GLFWwindow *backup_current_context = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		glfwMakeContextCurrent(backup_current_context);
	}
}

RAGE_gui::~RAGE_gui()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}