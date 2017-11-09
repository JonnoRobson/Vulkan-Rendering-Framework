#include "light.h"

Light::Light()
{
	position_ = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
	direction_ = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
	color_ = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	intensity_ = 0.0f;
	range_ = 0.0f;
	type_ = 0.0f;
	shadows_enabled_ = false;
}