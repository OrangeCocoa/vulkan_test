#pragma once

#include<Windows.h>

namespace Settings
{
	template <class T>
	constexpr T window_width = 640;

	template <class T>
	constexpr T window_height = 480;

	template <class T>
	constexpr T application_name = "Vulkan"; // caption

	struct Window
	{
		int	width;
		int	height;
		int	fullscreen;
		int	vsync;
	};

	struct Camera
	{
		union
		{
			float		fov_H;
			float		fov_V;
		};
		float		near_plane;
		float		far_plane;
		float		aspect;
		float		x, y, z;
		float		yaw, pitch;
	};

	struct ShadowMap
	{
		unsigned dimension;
	};

	struct PostProcess
	{
		struct Bloom
		{
			float	threshold_brdf;
			float	threshold_phong;
			int     blur_pass_count;
		} bloom;

		struct Tonemapping
		{
			float	exposure;
		} tonemapping;

		bool HDR_enabled = false;
	};

	struct Rendering
	{
		ShadowMap	shadow_map;
		PostProcess post_process;
		bool		use_deferred_rendering;
		bool		use_BRDF_lighting;
		bool		ambient_occlusion;
		bool		enable_environment_lighting;
		bool		pre_load_environment_maps;
	};

	struct Engine
	{
		Window window;
		Rendering rendering;
		int initialize_scene;
	};
}