#ifndef __ALFINA_RENDER_CAMERA_H__
#define __ALFINA_RENDER_CAMERA_H__

#include "engine/math/math.h"

namespace al::engine
{
	class PerspectiveRenderCamera
	{
	public:
		PerspectiveRenderCamera();

		PerspectiveRenderCamera
		(
			Transform	_transform,
			float2		_aspectRatio,
			float		_nearPlane,
			float		_farPlane,
			float		_fovDeg
		);

		float4x4 get_projection() const;
		float4x4 get_view() const;

		void set_position(const float3& position);
		void look_at(const float3& target, const float3& up);

	private:
		Transform	transform;
		float		aspectRatio;
		float		nearPlane;
		float		farPlane;
		float		fovDeg;
	};
}

#if defined(AL_UNITY_BUILD)
#	include "render_camera.cpp"
#else

#endif

#endif