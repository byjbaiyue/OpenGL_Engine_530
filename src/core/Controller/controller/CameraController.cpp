#include "CameraController.h"
#include "InputManager.h"
#include <iostream>
CameraController::CameraController(Camera* camera, int priority, const std::string& name)
	:ControllerBase(name, priority), camera(camera)
{

}

void CameraController::processInput(float deltaTime)
{
	keyInput();
	mouseInput(); 
	mouseScrollInput();
}

void CameraController::update(float deltaTime) {
	cameraMove(deltaTime);
	cameraRotate();
	cameraScroll();
	camera->update();
}

void CameraController::keyInput() {
	InputManager& input_manager = InputManager::GetInstance();

	key_states.forward = input_manager.IsKeyPressedAsFrame(KeyCode::W) || input_manager.IsKeyPressed(KeyCode::W);
	key_states.backward = input_manager.IsKeyPressedAsFrame(KeyCode::S) || input_manager.IsKeyPressed(KeyCode::S);
	key_states.left = input_manager.IsKeyPressedAsFrame(KeyCode::A) || input_manager.IsKeyPressed(KeyCode::A);
	key_states.right = input_manager.IsKeyPressedAsFrame(KeyCode::D) || input_manager.IsKeyPressed(KeyCode::D);
	key_states.up = input_manager.IsKeyPressedAsFrame(KeyCode::SPACE) || input_manager.IsKeyPressed(KeyCode::SPACE);
	key_states.down = input_manager.IsKeyPressedAsFrame(KeyCode::LEFT_SHIFT) || input_manager.IsKeyPressed(KeyCode::LEFT_SHIFT);

}

void CameraController::mouseInput() {
	InputManager& input_manager = InputManager::GetInstance();

	mouse_button_states.left = input_manager.IsMouseButtonPressed(MouseButton::LEFT);
	mouse_button_states.right = input_manager.IsMouseButtonPressed(MouseButton::RIGHT);
	mouse_button_states.middle = input_manager.IsMouseButtonPressed(MouseButton::MIDDLE);

	mouse_move_data = input_manager.GetMouseMoveData();
}

void CameraController::mouseScrollInput()
{
	InputManager& input_manager = InputManager::GetInstance();

	mouse_scroll_data -= input_manager.GetMouseScroll();
	
}

void CameraController::cameraMove(float dt) {

	if (key_states.forward) camera->setPos(camera->getPos() + camera->getFront() * (double)speed * (double)dt);
	if (key_states.backward) camera->setPos(camera->getPos() - camera->getFront() * (double)speed * (double)dt);
	if (key_states.left) camera->setPos(camera->getPos() - camera->getRight() * (double)(double)speed * (double)dt);
	if (key_states.right) camera->setPos(camera->getPos() + camera->getRight() * (double)speed * (double)dt);
	if (key_states.up) camera->setPos(camera->getPos() + camera->getUp() * (double)speed * (double)dt);
	if (key_states.down) camera->setPos(camera->getPos() - camera->getUp() * (double)speed * (double)dt);
}

void CameraController::cameraRotate() {
	if (mouse_button_states.left) {
		// 1. 计算目标旋转值（和原逻辑一致，保证基础速度）
		float target_yaw = camera->getYaw() - mouse_move_data.offset.x * rotation_speed;
		float target_pitch = camera->getPitch() - mouse_move_data.offset.y * rotation_speed;

		// 3. 优化的平滑插值：提高每帧的插值比例，保证速度
		float current_yaw = camera->getYaw();
		float current_pitch = camera->getPitch();

		// 关键修改：插值因子改用固定比例 + 时间补偿，而非 dt*固定值
		// smooth_factor建议值：0.1~0.3（值越大越灵敏，越小越平滑）
		const float smooth_factor = 0.1f;
		float smooth_yaw = current_yaw + (target_yaw - current_yaw) * smooth_factor;
		float smooth_pitch = current_pitch + (target_pitch - current_pitch) * smooth_factor;

		// 4. 设置平滑后的旋转值
		camera->setYawANDPitch(smooth_yaw, smooth_pitch);
	}
}

void CameraController::cameraScroll()
{
	if (mouse_scroll_data != 0.0f) {
		float current_fov = camera->getFov();

		// 计算当前视距（假设物体大小为2个单位）
		float current_distance = 1.0f / tan(glm::radians(current_fov) / 2.0f);

		// 关键：根据当前距离调整缩放速度
		// 距离越远，步长越大；距离越近，步长越小
		float zoom_factor = 1.0f - mouse_scroll_data * scroll_speed * 0.1f;
		float target_distance = current_distance * zoom_factor;

		// 确保不反转方向
		target_distance = glm::max(target_distance, 0.01f);

		// 反算FOV
		float target_fov = glm::degrees(2.0f * atan(1.0f / target_distance));
		target_fov = glm::clamp(target_fov, 0.1f, 179.0f);

		camera->setProjection(target_fov,
			(float)InputManager::GetInstance().GetWindowSize().x / InputManager::GetInstance().GetWindowSize().y,
			camera->getNearFar().x, camera->getNearFar().y);

		mouse_scroll_data = 0.0f;
	}
}
