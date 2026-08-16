#pragma once

#include <imgui.h>
#include <unordered_map>
#include <chrono>
#include <string>

namespace FrameWork
{
	namespace ImGui
	{
		// Sistema de animação para widgets
		class AnimationManager
		{
		public:
			// Obtém valor animado (0.0 a 1.0)
			static float GetAnimationValue(const std::string& id, float duration = 0.3f);

			// Reseta animação
			static void ResetAnimation(const std::string& id);

			// Limpa todas as animações
			static void ClearAnimations();

		private:
			struct AnimationState
			{
				std::chrono::steady_clock::time_point startTime;
				float duration;
				bool isActive;
			};

			static std::unordered_map<std::string, AnimationState> animations;
		};

		// Checkbox com animação
		bool AnimatedCheckbox(const char* label, bool* v, float animationDuration = 0.3f);

		// Button com animação
		bool AnimatedButton(const char* label, const ImVec2& size = ImVec2(0, 0), float animationDuration = 0.3f);

		// Slider com animação
		bool AnimatedSliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d", float animationDuration = 0.3f);

		bool AnimatedSliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", float animationDuration = 0.3f);

		// KeyBind com animação
		bool AnimatedKeyBind(const char* label, int* key, float animationDuration = 0.3f);

		// Helpers para easing
		namespace Easing
		{
			// Linear
			float Linear(float t);

			// Ease In Out Quad
			float EaseInOutQuad(float t);

			// Ease Out Cubic
			float EaseOutCubic(float t);

			// Ease In Out Cubic
			float EaseInOutCubic(float t);

			// Ease Out Elastic
			float EaseOutElastic(float t);
		}
	}
}
