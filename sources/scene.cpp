#include "scene.hpp"

Scene::Scene() = default;
Scene::~Scene() = default;

std::shared_ptr<Scene> Scene::create() {
    return std::make_shared<Scene>();
}