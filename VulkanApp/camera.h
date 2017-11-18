#ifndef _CAMERA_H_
#define _CAMERA_H_

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>

class Camera
{
public:
	Camera();

	void MoveForward(float speed);
	void MoveBackward(float speed);
	void MoveLeft(float speed);
	void MoveRight(float speed);

	void TurnUp(float speed) { rotation_.x += speed; }
	void TurnDown(float speed) { rotation_.x -= speed; }
	void TurnLeft(float speed) { rotation_.z += speed; }
	void TurnRight(float speed) { rotation_.z -= speed; }

	inline void SetPosition(glm::vec3 pos) { position_ = pos; }
	inline glm::vec3 GetPosition() { return position_; }

	inline void SetRotation(glm::vec3 rot) { rotation_ = rot; }
	inline glm::vec3 GetRotation() { return rotation_; }

	glm::mat4 GetViewMatrix();

protected:
	glm::vec3 position_;
	glm::vec3 rotation_;


};

#endif