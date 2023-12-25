#include "RAGE.hpp"
#include "RAGE_inspector.hpp"

void RAGE_inspector::draw(RAGE *rage)
{
	ImGui::Begin("Inspector", NULL, ImGuiWindowFlags_NoCollapse);
	ImGui::Text("Scene object count: %zu", rage->scene.get_objects()->size());
	std::vector<RAGE_object *> *selected_objects = rage->scene.get_selected_objects();
	ImGui::Text("Selected object count: %zu", selected_objects->size());
	for (int i = 0; i < selected_objects->size(); i++)
	{
		RAGE_object *selected_object = selected_objects->at(i);
		ImGui::Text("Selected object name: %s", selected_object->get_name().c_str());
		ImGui::Text("UUID: %llu", (unsigned long long)selected_object);

		glm::vec3 position = selected_object->get_position();
		std::string label = "Position" + std::to_string(i);
		float drag_speed = 0.1f;
		if (ImGui::DragFloat3(label.c_str(), &position.x, drag_speed) == true)
			selected_object->set_position(position);
	}
	ImGui::End();
}