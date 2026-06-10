#include "SceneManager.h"
#include <Windows.h> // OutputDebugStringA

namespace Engine {


SceneManager* SceneManager::instance_ = nullptr;

SceneManager* SceneManager::GetInstance() { return instance_; }

void SceneManager::Register(const std::string& name, Factory factory) { factories_[name] = std::move(factory); }

bool SceneManager::Has(const std::string& name) const { return factories_.find(name) != factories_.end(); }

std::string SceneManager::FirstRegisteredName() const {
	if (factories_.empty())
		return {};
	// unordered_map は順序不定だが「何も無いよりマシ」な自動起動用
	return factories_.begin()->first;
}

std::vector<std::string> SceneManager::RegisteredNames() const {
	std::vector<std::string> out;
	out.reserve(factories_.size());
	for (auto& kv : factories_)
		out.push_back(kv.first);
	return out;
}

bool SceneManager::Change(const std::string& name, const SceneParameters& params) {
	auto it = factories_.find(name);
	if (it == factories_.end()) {
		OutputDebugStringA("[SceneManager] Change failed: not registered\n");
		return false;
	}

	current_ = it->second();
	currentName_ = name;

	if (current_) {
		OutputDebugStringA("[SceneManager] Initialize scene\n");
		current_->Initialize(dx_, params);
	}

	pendingNext_.clear();
	pendingParams_ = {};
	return true;
}

void SceneManager::RequestChange(const std::string& name, const SceneParameters& params) {
	pendingNext_ = name;
	pendingParams_ = params;
}

void SceneManager::Update() {
	if (!pendingNext_.empty()) {
		Change(pendingNext_, pendingParams_);
	}

	if (current_) {
		current_->Update();

		if (current_->IsEnd()) {
			const std::string next = current_->Next();
			if (!next.empty()) {
				Change(next);
			}
		}
	}
}

void SceneManager::Draw() {
	if (current_) {
		current_->Draw();
	}
}

void SceneManager::Clear() {
	current_.reset();
	currentName_.clear();
	pendingNext_.clear();
	factories_.clear();
}

} // namespace Engine
