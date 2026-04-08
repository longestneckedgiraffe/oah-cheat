#pragma once

#include <array>
#include <cmath>
#include <d3dx9math.h>
#include "../Libs/ImGui/imgui.h"

template <typename T>
struct ColorRGBA
{
	T r, g, b, a;

	constexpr ColorRGBA() : r(0), g(0), b(0), a(0) {}
	constexpr ColorRGBA(T r, T g, T b, T a = 255) : r(r), g(g), b(b), a(a) {}

	ImVec4 imVec4() const { return { r / 255.f, g / 255.f, b / 255.f, a / 255.f }; }
	ImU32 imGui() const { return IM_COL32(r, g, b, a); }
	D3DXCOLOR d3d() const { return D3DXCOLOR(r / 255.f, g / 255.f, b / 255.f, a / 255.f); }
};

namespace Colors
{
	constexpr ColorRGBA<float> mainFontColor = { 255.f, 255.f, 255.f, 255.f };
	constexpr ColorRGBA<float> windowBgColor = { 15.f, 15.f, 15.f, 255.f };
	constexpr ColorRGBA<float> childBgColor = { 25.f, 25.f, 25.f, 255.f };
	constexpr ColorRGBA<float> buttonColor = { 45.f, 45.f, 45.f, 255.f };
	constexpr ColorRGBA<float> buttonHoveredColor = { 55.f, 55.f, 55.f, 255.f };
	constexpr ColorRGBA<float> buttonActiveColor = { 65.f, 65.f, 65.f, 255.f };
	constexpr ColorRGBA<float> frameColor = { 55.f, 55.f, 55.f, 255.f };
	constexpr ColorRGBA<float> headerColor = { 45.f, 45.f, 45.f, 255.f };

	constexpr ColorRGBA<float> UltramarineBlue = { 65.f, 102.f, 245.f, 255.f };
	constexpr ColorRGBA<float> UltramarineBlueLow = { 65.f, 102.f, 245.f, 100.f };
}
