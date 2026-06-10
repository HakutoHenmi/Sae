					ImGui::DragFloat("Outer Cos##SL", &sl.outerCos, 0.001f, 0.0f, 1.0f);
					ImGui::DragFloat3("Attenuation##SL", &sl.atten.x, 0.01f);
					if (ImGui::Button("Remove##SL")) {
						obj.spotLights.erase(obj.spotLights.begin() + ci);
						ImGui::TreePop(); ImGui::PopID(); goto end_comp;
					}
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			// Variables
			for (size_t ci = 0; ci < obj.variables.size(); ++ci) {
				auto& vc = obj.variables[ci];
				ImGui::PushID(24000 + (int)ci);
				if (ImGui::TreeNode("Variables")) {
					std::string dKey;
					for (auto& [key, val] : vc.values) {
						ImGui::Text("%s:", key.c_str()); ImGui::SameLine(100);
						ImGui::DragFloat(("##v" + key).c_str(), &val, 0.1f);
						ImGui::SameLine();
						if (ImGui::Button(("x##v" + key).c_str())) dKey = key;
					}
					if (!dKey.empty()) vc.values.erase(dKey);
					if (ImGui::Button("Remove##VC")) {
						obj.variables.erase(obj.variables.begin() + ci);
						ImGui::TreePop(); ImGui::PopID(); goto end_comp;
					}
					ImGui::TreePop();
				}
				ImGui::PopID();
			}

		end_comp:
			ImGui::Separator();
			if (ImGui::Button("Add Component")) ImGui::OpenPopup("AddComp");
			if (ImGui::BeginPopup("AddComp")) {
				if (ImGui::MenuItem("MeshRenderer")) obj.meshRenderers.push_back({});
				if (ImGui::MenuItem("BoxCollider")) obj.boxColliders.push_back({});
				if (ImGui::MenuItem("Rigidbody")) obj.rigidbodies.push_back({});
				if (ImGui::MenuItem("ParticleEmitter")) { ParticleEmitterComponent pe; pe.emitter.Initialize(*Engine::Renderer::GetInstance(), "New"); obj.particleEmitters.push_back(pe); }
				if (ImGui::MenuItem("AudioSource")) obj.audioSources.push_back({});
				if (ImGui::MenuItem("River")) obj.rivers.push_back({});
				if (ImGui::MenuItem("Health")) obj.healths.push_back({});
				if (ImGui::MenuItem("Script")) obj.scripts.push_back({});
				if (ImGui::MenuItem("Variables")) obj.variables.push_back({});
				if (ImGui::MenuItem("WorldSpaceUI")) obj.worldSpaceUIs.push_back({});
				ImGui::EndPopup();
			}
		}

		ImGui::Separator();
		const char* gModes[] = {"Translate (T)", "Rotate (R)", "Scale (S)"};
		ImGui::Text("Gizmo: %s", gModes[(int)currentGizmoMode]);
	} else {
		ImGui::Text("No Object Selected");
	}
	ImGui::End();
}

void EditorUI::ShowProject(Engine::Renderer* renderer, MainScene* scene) {
	(void)renderer;
	(void)scene;
	ImGui::Begin("Project");

	static std::string currentDir = "Resources";
	static float iconSize = 80.0f;
	if (!fs::exists(currentDir)) currentDir = "Resources";

	// Breadcrumbs
	{
		std::istringstream iss(currentDir);
		std::string token, accumulated;
		bool first = true;
		while (std::getline(iss, token, '\\')) {
			if (!first) { ImGui::SameLine(); ImGui::Text(">"); ImGui::SameLine(); }
			accumulated += (first ? "" : "\\") + token;
			if (ImGui::SmallButton(token.c_str())) currentDir = accumulated;
			first = false;
		}
	}
	ImGui::Separator();

	float panelWidth = ImGui::GetContentRegionAvail().x;
	int columns = (int)(panelWidth / (iconSize + 10));
	if (columns < 1) columns = 1;

	ImGui::Columns(columns, nullptr, false);
	if (currentDir != "Resources") {
		if (ImGui::Button("..", ImVec2(iconSize, iconSize))) {
			currentDir = fs::path(currentDir).parent_path().string();
		}
		ImGui::NextColumn();
	}

	for (auto& entry : fs::directory_iterator(currentDir)) {
		std::string name = entry.path().filename().string();
		bool isDir = entry.is_directory();
		if (isDir) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.1f, 1));
		if (ImGui::Button(name.c_str(), ImVec2(iconSize, iconSize))) {
			if (isDir) currentDir = entry.path().string();
		}
		if (isDir) ImGui::PopStyleColor();

		if (!isDir && ImGui::BeginDragDropSource()) {
			std::string path = entry.path().string();
			ImGui::SetDragDropPayload("RESOURCE_PATH", path.c_str(), path.size() + 1);
			ImGui::Text("%s", name.c_str());
			ImGui::EndDragDropSource();
		}
		ImGui::TextWrapped("%s", name.c_str());
		ImGui::NextColumn();
	}
	ImGui::Columns(1);
	ImGui::End();
}

void EditorUI::DrawSelectionGizmo(Engine::Renderer* renderer, MainScene* scene) {
    if (!scene) return;
    for (int idx : scene->selectedIndices_) {
        if (idx < 0 || idx >= (int)scene->objects_.size()) continue;
        auto& obj = scene->objects_[idx];
        Engine::Vector3 pos = {obj.translate.x, obj.translate.y, obj.translate.z};
        const float al = 2.0f, ar = 0.3f;

        auto axisColor = [](int axis, int dragAxis) -> Engine::Vector4 {
            bool active = (dragAxis == axis);
            switch (axis) {
                case 0: return active ? Engine::Vector4{1.0f, 0.5f, 0.5f, 1.0f} : Engine::Vector4{1.0f, 0.2f, 0.2f, 1.0f};
                case 1: return active ? Engine::Vector4{0.5f, 1.0f, 0.5f, 1.0f} : Engine::Vector4{0.2f, 1.0f, 0.2f, 1.0f};
                case 2: return active ? Engine::Vector4{0.5f, 0.5f, 1.0f, 1.0f} : Engine::Vector4{0.2f, 0.2f, 1.0f, 1.0f};
                default: return {1, 1, 1, 1};
            }
        };

        int dAxis = (gizmoDragging && idx == scene->selectedObjectIndex_) ? gizmoDragAxis : -1;
        auto cX = axisColor(0, dAxis), cY = axisColor(1, dAxis), cZ = axisColor(2, dAxis);

        if (currentGizmoMode == GizmoMode::Translate) {
            renderer->DrawLine3D(pos, {pos.x + al, pos.y, pos.z}, cX);
            renderer->DrawLine3D({pos.x + al, pos.y, pos.z}, {pos.x + al - ar, pos.y + ar * .4f, pos.z}, cX);
            renderer->DrawLine3D({pos.x + al, pos.y, pos.z}, {pos.x + al - ar, pos.y - ar * .4f, pos.z}, cX);
            renderer->DrawLine3D(pos, {pos.x, pos.y + al, pos.z}, cY);
            renderer->DrawLine3D({pos.x, pos.y + al, pos.z}, {pos.x + ar * .4f, pos.y + al - ar, pos.z}, cY);
            renderer->DrawLine3D({pos.x, pos.y + al, pos.z}, {pos.x - ar * .4f, pos.y + al - ar, pos.z}, cY);
            renderer->DrawLine3D(pos, {pos.x, pos.y, pos.z + al}, cZ);
            renderer->DrawLine3D({pos.x, pos.y, pos.z + al}, {pos.x, pos.y + ar * .4f, pos.z + al - ar}, cZ);
            renderer->DrawLine3D({pos.x, pos.y, pos.z + al}, {pos.x, pos.y - ar * .4f, pos.z + al - ar}, cZ);
        } else if (currentGizmoMode == GizmoMode::Rotate) {
			const int seg = 32;
			const float rad = 1.5f;
			for (int i = 0; i < seg; ++i) {
				float a0 = (float)i / seg * DirectX::XM_2PI, a1 = (float)(i + 1) / seg * DirectX::XM_2PI;
				renderer->DrawLine3D({pos.x, pos.y + cosf(a0) * rad, pos.z + sinf(a0) * rad}, {pos.x, pos.y + cosf(a1) * rad, pos.z + sinf(a1) * rad}, cX);
				renderer->DrawLine3D({pos.x + cosf(a0) * rad, pos.y, pos.z + sinf(a0) * rad}, {pos.x + cosf(a1) * rad, pos.y, pos.z + sinf(a1) * rad}, cY);
				renderer->DrawLine3D({pos.x + cosf(a0) * rad, pos.y + sinf(a0) * rad, pos.z}, {pos.x + cosf(a1) * rad, pos.y + sinf(a1) * rad, pos.z}, cZ);
			}
        } else {
            float e = 0.15f;
            renderer->DrawLine3D(pos, {pos.x + al, pos.y, pos.z}, cX);
            renderer->DrawLine3D({pos.x + al - e, pos.y - e, pos.z}, {pos.x + al + e, pos.y + e, pos.z}, cX);
            renderer->DrawLine3D({pos.x + al + e, pos.y - e, pos.z}, {pos.x + al - e, pos.y + e, pos.z}, cX);
            renderer->DrawLine3D(pos, {pos.x, pos.y + al, pos.z}, cY);
            renderer->DrawLine3D({pos.x - e, pos.y + al - e, pos.z}, {pos.x + e, pos.y + al + e, pos.z}, cY);
            renderer->DrawLine3D({pos.x + e, pos.y + al - e, pos.z}, {pos.x - e, pos.y + al + e, pos.z}, cY);
            renderer->DrawLine3D(pos, {pos.x, pos.y, pos.z + al}, cZ);
            renderer->DrawLine3D({pos.x, pos.y - e, pos.z + al - e}, {pos.x, pos.y + e, pos.z + al + e}, cZ);
            renderer->DrawLine3D({pos.x, pos.y + e, pos.z + al - e}, {pos.x, pos.y - e, pos.z + al + e}, cZ);
        }

        float sx = obj.scale.x * 0.5f, sy = obj.scale.y * 0.5f, sz = obj.scale.z * 0.5f;
        Engine::Vector4 hlColor = {1.0f, 0.85f, 0.0f, 0.9f};
        Engine::Vector3 v[8] = {
            {pos.x - sx, pos.y - sy, pos.z - sz}, {pos.x + sx, pos.y - sy, pos.z - sz},
            {pos.x + sx, pos.y + sy, pos.z - sz}, {pos.x - sx, pos.y + sy, pos.z - sz},
            {pos.x - sx, pos.y - sy, pos.z + sz}, {pos.x + sx, pos.y - sy, pos.z + sz},
            {pos.x + sx, pos.y + sy, pos.z + sz}, {pos.x - sx, pos.y + sy, pos.z + sz}
        };
        int edges[][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
        for (auto& eg : edges) renderer->DrawLine3D(v[eg[0]], v[eg[1]], hlColor);

        for (const auto& bc : obj.boxColliders) {
            if (!bc.enabled) continue;
            float csx = bc.size.x * 0.5f * obj.scale.x, csy = bc.size.y * 0.5f * obj.scale.y, csz = bc.size.z * 0.5f * obj.scale.z;
            Engine::Vector3 cp = {pos.x + bc.center.x * obj.scale.x, pos.y + bc.center.y * obj.scale.y, pos.z + bc.center.z * obj.scale.z};
            Engine::Vector3 cv[8] = {
                {cp.x - csx, cp.y - csy, cp.z - csz}, {cp.x + csx, cp.y - csy, cp.z - csz},
                {cp.x + csx, cp.y + sy, cp.z - csz}, {cp.x - csx, cp.y + sy, cp.z - csz},
                {cp.x - csx, cp.y - sy, cp.z + csz}, {cp.x + csx, cp.y - sy, cp.z + csz},
                {cp.x + csx, cp.y + sy, cp.z + csz}, {cp.x - csx, cp.y + sy, cp.z + csz}
            };
            for (auto& eg : edges) renderer->DrawLine3D(cv[eg[0]], cv[eg[1]], {0.2f, 1.0f, 0.2f, 0.8f});
        }
    }
}

void EditorUI::ShowAnimationWindow(Engine::Renderer* renderer, MainScene* scene) {
    (void)renderer;
    ImGui::Begin("Animation");
    if (scene && scene->selectedObjectIndex_ >= 0 && scene->selectedObjectIndex_ < (int)scene->objects_.size()) {
        auto& obj = scene->objects_[scene->selectedObjectIndex_];
        if (!obj.animators.empty()) {
            auto& anim = obj.animators[0];
            ImGui::Text("Selected: %s (Animator)", obj.name.c_str());
            ImGui::Separator();
            auto* r = Engine::Renderer::GetInstance();
            auto* m = r->GetModel(obj.modelHandle);
            if (m) {
                const auto& data = m->GetData();
                if (!data.animations.empty()) {
                    if (ImGui::BeginCombo("Clips", anim.currentAnimation.c_str())) {
                        for (const auto& a : data.animations) {
                            if (ImGui::Selectable(a.name.c_str(), anim.currentAnimation == a.name)) {
                                anim.currentAnimation = a.name;
                                anim.time = 0.0f;
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::Checkbox("Playing", &anim.isPlaying);
                    ImGui::SameLine();
                    ImGui::Checkbox("Loop", &anim.loop);
                    ImGui::DragFloat("Speed", &anim.speed, 0.05f);
                } else ImGui::Text("No animations.");
            } else ImGui::Text("No model.");
        } else ImGui::Text("No Animator.");
    }
    ImGui::End();
}

void EditorUI::ShowPlayModeMonitor(MainScene* scene) {
    if (!scene || !scene->IsPlaying()) return;
    ImGui::Begin("Play Mode Monitor");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Separator();
    if (ImGui::BeginTable("Mon", 3, ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Name"); ImGui::TableSetupColumn("Pos"); ImGui::TableSetupColumn("HP");
        ImGui::TableHeadersRow();
        for (const auto& obj : scene->GetObjects()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%s", obj.name.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f,%.1f,%.1f", obj.translate.x, obj.translate.y, obj.translate.z);
            ImGui::TableSetColumnIndex(2);
            if (!obj.healths.empty()) ImGui::Text("%.1f/%.1f", obj.healths[0].hp, obj.healths[0].maxHp);
            else ImGui::Text("-");
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

void EditorUI::ShowSceneSettings(Engine::Renderer* renderer) {
    ImGui::Begin("Scene Settings");
    auto pp = renderer->GetPostProcessParams();
    bool ch = false;

    // ☁E�Eストエフェクト種類�EドロチE�Eダウン�E�常時表示�E�E    static const char* ppTypes[] = { "None", "Default(CRT)", "Rich", "Anime", "Grayscale", "Smoothing", "GaussianFilter", "OutlinePost", "RadialBlur", "Random" };
    static int currentPP = 1; // チE��ォルト�ECRT
    if (ImGui::BeginCombo("PostEffect", ppTypes[currentPP])) {
        for (int n = 0; n < IM_ARRAYSIZE(ppTypes); n++) {
            bool isSelected = (currentPP == n);
            if (ImGui::Selectable(ppTypes[n], isSelected)) {
                currentPP = n;
                if (currentPP == 0) {
                    // None: ポストエフェクト無効
                    renderer->SetPostProcessEnabled(false);
                } else if (currentPP == 1) {
                    // Default(CRT): 初期PSOに戻ぁE                    renderer->SetPostEffect("");
                    renderer->SetPostProcessEnabled(true);
                } else {
                    renderer->SetPostEffect(ppTypes[currentPP]);
                }
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // パラメータ�E��Eストエフェクト有効時�Eみ表示�E�E    if (currentPP > 0) {
        ImGui::Separator();
        ch |= ImGui::DragFloat("Vignette", &pp.vignette, 0.01f);
        ch |= ImGui::DragFloat("Noise", &pp.noiseStrength, 0.01f);
        ch |= ImGui::DragFloat("Distortion", &pp.distortion, 0.001f);
        ch |= ImGui::DragFloat("ChromaShift", &pp.chromaShift, 0.0001f);
        ch |= ImGui::DragFloat("Scanline", &pp.scanline, 0.01f);
        ch |= ImGui::DragFloat("SAN", &pp.san, 0.01f, 0.0f, 1.0f);
    }
    if (ch) renderer->SetPostProcessParams(pp);
    ImGui::End();
}


void EditorUI::ShowConsole() {
    ImGui::Begin("Console");
    if (ImGui::Button("Clear")) consoleLog.clear();
    ImGui::BeginChild("LogScroll");
    for (const auto& e : consoleLog) {
        ImVec4 col = (e.level == LogLevel::Error) ? ImVec4(1,0,0,1) : (e.level == LogLevel::Warning ? ImVec4(1,1,0,1) : ImVec4(1,1,1,1));
        ImGui::TextColored(col, "%s", e.message.c_str());
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace Game
