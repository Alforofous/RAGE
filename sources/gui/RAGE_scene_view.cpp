#include "RAGE.hpp"
#include "RAGE_scene_view.hpp"

RAGE_scene_view::RAGE_scene_view()
{
	glGenFramebuffers(1, &framebuffer);
	glGenRenderbuffers(1, &depthbuffer);
	glGenTextures(1, &texture);
	glfwGetWindowSize(glfwGetCurrentContext(), &window_size.x, &window_size.y);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
}

void RAGE_scene_view::draw(RAGE *rage)
{
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthbuffer);
	glBindTexture(GL_TEXTURE_2D, texture);

	glm::ivec2 framebuffer_size;
	glfwGetFramebufferSize(rage->window->glfw_window, &framebuffer_size.x, &framebuffer_size.y);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, framebuffer_size.x, framebuffer_size.y);

	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthbuffer);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, framebuffer_size.x, framebuffer_size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "Framebuffer is not complete!" << std::endl;
		return;
	}

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	rage->scene.draw(rage);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	ImGui::Begin("Scene", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
	static bool first_time_open = true;
	if (first_time_open == true)
	{
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(this->window_size.x, this->window_size.y));
		first_time_open = false;
	}
	rage->camera.set_aspect_ratio(this->window_size);
	this->window_size = glm::ivec2(ImGui::GetWindowSize().x, ImGui::GetWindowSize().y);
	ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
	ImGui::Image((void *)(intptr_t)texture, canvas_sz, ImVec2(0, 1), ImVec2(1, 0));
	ImGui::End();
}

RAGE_scene_view::~RAGE_scene_view()
{
	glDeleteFramebuffers(1, &framebuffer);
	glDeleteRenderbuffers(1, &depthbuffer);
	glDeleteTextures(1, &texture);
}