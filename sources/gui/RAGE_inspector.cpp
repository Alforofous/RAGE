#include "RAGE.hpp"
#include "RAGE_inspector.hpp"

void RAGE_inspector::draw(RAGE *rage)
{
	ImGui::Begin("Inspector", NULL, ImGuiWindowFlags_NoCollapse);
	ImGui::Text("Scene object count: %zu", rage->scene.get_objects()->size());
	std::vector<RAGE_object *> *selected_objects = rage->scene.get_selected_objects();
	ImGui::Text("Selected object count: %zu", selected_objects->size());
	for (int i = 0; i < selected_objects->size(); i++)
		ImGui::Text("Selected object name: %s", selected_objects->at(i)->get_name().c_str());
	ImGui::End();
}