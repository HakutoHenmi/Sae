#pragma once
// ===============================
//  EventSystem : スクリプト間通信用メッセージバス
//  - シングルトン/static不使用。GameScene が所有する。
//  - Subscribe(name, callback) でイベント購読
//  - Emit(name, value) でイベント発火
//  - Clear() でシーン切り替え時にリセット
// ===============================
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <any>

namespace Engine {

class EventSystem {
public:
	// === float 値イベント ===
	using FloatCallback = std::function<void(float)>;

	void Subscribe(const std::string& eventName, FloatCallback callback) {
		floatListeners_[eventName].push_back(std::move(callback));
	}

	void Emit(const std::string& eventName, float value = 0.0f) {
		auto it = floatListeners_.find(eventName);
		if (it != floatListeners_.end()) {
			for (auto& cb : it->second) {
				cb(value);
			}
		}
	}

	// === 文字列値イベント ===
	using StringCallback = std::function<void(const std::string&)>;

	void SubscribeString(const std::string& eventName, StringCallback callback) {
		stringListeners_[eventName].push_back(std::move(callback));
	}

	void EmitString(const std::string& eventName, const std::string& value = "") {
		auto it = stringListeners_.find(eventName);
		if (it != stringListeners_.end()) {
			for (auto& cb : it->second) {
				cb(value);
			}
		}
	}

	// === 引数なしイベント（トリガー用） ===
	using VoidCallback = std::function<void()>;

	void SubscribeVoid(const std::string& eventName, VoidCallback callback) {
		voidListeners_[eventName].push_back(std::move(callback));
	}

	void EmitVoid(const std::string& eventName) {
		auto it = voidListeners_.find(eventName);
		if (it != voidListeners_.end()) {
			for (auto& cb : it->second) {
				cb();
			}
		}
	}

	// === 全リスナー解除（シーン切り替え時） ===
	void Clear() {
		floatListeners_.clear();
		stringListeners_.clear();
		voidListeners_.clear();
	}

	// === 特定イベントのリスナー解除 ===
	void Unsubscribe(const std::string& eventName) {
		floatListeners_.erase(eventName);
		stringListeners_.erase(eventName);
		voidListeners_.erase(eventName);
	}

private:
	std::unordered_map<std::string, std::vector<FloatCallback>> floatListeners_;
	std::unordered_map<std::string, std::vector<StringCallback>> stringListeners_;
	std::unordered_map<std::string, std::vector<VoidCallback>> voidListeners_;
};

} // namespace Engine
