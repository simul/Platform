#include "MouseHandler.h"

using namespace platform;
using namespace core;
using namespace crossplatform;

MouseHandler::MouseHandler(bool yv)
	:step_rotate_x(0)
	,step_rotate_y(0)
	,cameraMode(LOOKAROUND)
	,CameraDamping(1e5f)
	,minAlt(0.f)
	,maxAlt(10000.f)
	,y_vertical(yv)
	,aspect(1.f)
	,camera(NULL)
	,move_forward(false)
	,move_backward(false)
	,move_left(false)
	,move_right(false)
	,move_up(false)
	,move_down(false)
	,shift_down(false)
	,alt_down(false)
	,up_down_spd(0.f)
	,forward_back_spd(0.f)
	,right_left_spd(0.f)
{
	camera=new Camera();
	vec3 cameraPos(0,0,0);
	camera->SetPosition(cameraPos);
	vec3 lookAtPos(-1.0f, cameraPos.y, cameraPos.z);
	if(!y_vertical)
	{
		std::swap(lookAtPos.y,lookAtPos.z);
		std::swap(cameraPos.y,cameraPos.z);
	}
	vec3 y={0,1.f,0};
	vec3 z={0,0,1.f};
	vec3 vert = y_vertical ? y : z;
	camera->LookInDirection(lookAtPos-cameraPos,vert);
}

MouseHandler::~MouseHandler()
{
	delete camera;
}

void MouseHandler::SetSpeedFactor(float s)
{
	speed_factor=s;
}

float MouseHandler::GetSpeedFactor() const
{
    return speed_factor;
}

void MouseHandler::setCentre(const float *c)
{
	centre[0]=c[0];
	centre[1]=c[1];
	centre[2]=c[2];
	if(y_vertical)
		std::swap(centre[1],centre[2]);
}

void MouseHandler::setEnginePose(const posed& pose)
{
	enginePose=pose;
	enginePose.orientation.MakeUnit();
	enginePose.position *= 1000.0;
}

void MouseHandler::setLocalRadiusKmCallback(std::function<float(float)> pfn)
{
	LocalRadiusKmCallback=pfn;
}

void MouseHandler::setAltitudeRange(float m,float M)
{
	minAlt=m;
	maxAlt=M;
}


static float uu=750.f;
void MouseHandler::mouseMove(int x,int y)
{
	int dx=x-MouseX;
	int dy=y-MouseY;
	fDeltaX=dx/uu;
	fDeltaY=dy/uu;
	MouseX=x;
	MouseY=y;
	if (updateViews)
		updateViews();
}
#pragma optimize("",off)
vec3 MouseHandler::getMouseDirection(int x,int y,int viewport_x,int viewport_y) const
{
	float screenX = (float(x) / float(viewport_x) );
	float screenY = (float(y) / float(viewport_y) );
	float aspect=float(viewport_x)/float(viewport_y);
	vec3 dir=camera->ScreenPositionToDirection(screenX,screenY,aspect);
	return dir;
}

void MouseHandler::getMousePosition(int &x,int &y) const
{
	x=MouseX;
	y=MouseY;
}

bool MouseHandler::getLeftButton() const
{
	return (mouseButtons==core::MouseButtons::LeftButton);
}

bool MouseHandler::getRightButton() const
{
	return (mouseButtons==core::MouseButtons::RightButton);
}

bool MouseHandler::getMiddleButton() const
{
	return (mouseButtons==core::MouseButtons::MiddleButton);
}

void MouseHandler::mouseWheel(int delta,int modifiers)
{
	BaseMouseHandler::mouseWheel(delta,modifiers);
	if (modifiers == (int)KeyboardModifiers::Ctrl)
	{
		static float min_deg=5.0f;
		float fov = camera->GetHorizontalFieldOfViewDegrees();
		if(delta<0&&fov<180.f/1.1f)
			fov*=1.1f;
		else if(delta>0&&fov>min_deg*1.1f)
			fov/=1.1f;
		camera->SetHorizontalFieldOfViewDegrees(fov);
	}
	else
	{
		int mult = (modifiers==1)?3:1;
		if(delta>0)
			wheel_forward=5*mult;
		if (delta<0)
			wheel_backward=5*mult;
	}
	camera->SetVerticalFieldOfViewDegrees(0);
	if (updateViews)
		updateViews();
}

platform::crossplatform::Camera *MouseHandler::GetCamera()
{
	return camera;
}

float MouseHandler::getFov() const 
{
	return camera->GetHorizontalFieldOfViewDegrees();
}

void MouseHandler::setFov(float f)
{
	if(f<0.1f)
		f=0.1f;
	if(f>100.f)
		f=100.f;
	
	camera->SetHorizontalFieldOfViewDegrees(f);
}

float MouseHandler::GetSpeed() const
{
	return sqrt(up_down_spd*up_down_spd+forward_back_spd*forward_back_spd+right_left_spd*right_left_spd);
}
void MouseHandler::Update(float time_step)
{
	math::Vector3 offset_camspace;
	if(cameraMode==CENTRED)
	{
		math::Vector3 pos=camera->GetPosition();
		math::Vector3 dir=pos;
		dir-=centre;
		camera->Orientation.GlobalToLocalDirection(offset_camspace,dir);
	}
	if (cameraSpatial == EUCLIDEAN)
	{
		float cam_spd=speed_factor*(shift_down?shift_multiplier:1.f);
		math::Vector3 pos=camera->GetPosition();

		float retain=1.f/(1.f+CameraDamping*time_step);
		float introduce=1.f-retain;

		forward_back_spd*=retain;
		right_left_spd*=retain;
		up_down_spd*=retain;
		if(move_forward||wheel_forward)
			forward_back_spd+=cam_spd*introduce;
		if(move_backward||wheel_backward)
			forward_back_spd -= cam_spd * introduce;
		forward_back_spd += cam_spd * introduce * float(wheel_forward - wheel_backward);
		wheel_forward=wheel_backward = 0;
		if(move_left)
			right_left_spd-=cam_spd*introduce;
		if(move_right)
			right_left_spd+=cam_spd*introduce;
		if(move_up)
			up_down_spd+=cam_spd*introduce;
		if(move_down)
			up_down_spd-=cam_spd*introduce;
		if (mouseButtons == core::MouseButtons::MiddleButton || mouseButtons == (core::MouseButtons::LeftButton | core::MouseButtons::RightButton))
		{
			static float slide_spd=100.f;
			right_left_spd	+=slide_spd*fDeltaX*cam_spd*introduce;
			up_down_spd		-=slide_spd*fDeltaY*cam_spd*introduce;
		}

		math::Vector3 view_dir = camera->Orientation.Tz();
		view_dir.Normalize();

		pos -= forward_back_spd * time_step * view_dir;
		pos += right_left_spd * time_step * camera->Orientation.Tx();
		(y_vertical ? pos.y : pos.z) += up_down_spd * time_step;
		(y_vertical ? pos.y : pos.z) = std::clamp((y_vertical ? pos.y : pos.z), minAlt, maxAlt);

		camera->SetPosition(pos.Values);

		static float x_rotate=0.f;
		static float y_rotate=0.f;
		static float z_rotate=0.f;
		x_rotate*=retain;
		y_rotate*=retain;
		z_rotate*=retain;
		float tilt = y_vertical ? asin(camera->Orientation.Tx().y) : asin(camera->Orientation.Tx().z);
		if(mouseButtons == core::MouseButtons::RightButton)
		{
			if(!alt_down)
			{
				if(cameraMode==FLYING)
					y_rotate-=fDeltaY*introduce;
				else
					y_rotate+=fDeltaY*introduce;
				if(cameraMode==FLYING)
				{
					static float rr=0.5f;
					z_rotate-=fDeltaX*introduce*rr;
				}
				else
				{
					static float xx=1.5f;
					x_rotate+=fDeltaX*introduce*xx;
				}
			}
		}
		if(mouseButtons == core::MouseButtons::MiddleButton)
		{
			//z_rotate-=fDeltaX*introduce;
		}
		static float cc=0.01f;
		if(cameraMode==FLYING)
		{
			x_rotate-=tilt*cc*introduce;
		}
		fDeltaX=0.f;
		fDeltaY=0.f;

		math::Vector3 vertical(0.0f, 0.0f, 1.0f);
		if(y_vertical)
			vertical.Define(vertical.x, vertical.z, vertical.y);

		static float sr=0.1f;
		math::Vector3 del=vertical*(x_rotate+sr*step_rotate_x)*(-1.f);
		step_rotate_x=0;
		math::Vector3 dir=del;
		dir.Normalize();
		camera->Rotate(del.Magnitude(),dir.Values);

		del=camera->Orientation.Tx()*(y_rotate+sr*step_rotate_y)*(-1.f);
		step_rotate_y=0;
		dir=del;
		dir.Normalize();
		camera->Rotate(del.Magnitude(),dir.Values);

		del=math::Vector3(0.f,0.f,z_rotate);
		camera->LocalRotate(del.Values);

		static float correct_tilt=0.005f;
		dir=camera->Orientation.Tz();
		dir.Normalize();
		if(!alt_down)
			camera->Rotate(-correct_tilt * tilt, view_dir.Values);

	}
	else if (cameraSpatial == SPHERICAL)
	{
		float cam_spd = speed_factor * (shift_down ? shift_multiplier : 1.f);
		math::Vector3 pos = camera->GetPosition();
		{
			// Global spherical to local Euclidean.
			vec3d position;
			position.x = pos.x;
			position.y = y_vertical ? pos.z : pos.y;
			position.z = y_vertical ? pos.y : pos.z;
			position = enginePose.orientation.RotateVector(position);
			position += enginePose.position;

			float r = (float)length(position);
			float Lat_Rad = asinf((float)position.z / r);
			float Lon_Rad = atan2f((float)position.y, (float)position.x);
			//r = LocalRadiusKmCallback(Lat_Rad) * 1000.0f + y_vertical ? pos.y : pos.z;

			position.x = Lon_Rad * r;
			position.y = Lat_Rad * r;
			position.z = r - (float)length(enginePose.position);

			pos.x = (float)position.x;
			y_vertical ? pos.z : pos.y = (float)position.y;
			y_vertical ? pos.y : pos.z = (float)position.z;

			//camera->Orientation.DefineFromCameraEuler(0.0f, -Lat_Rad, Lon_Rad);
		}

		float retain = 1.f / (1.f + CameraDamping * time_step);
		float introduce = 1.f - retain;

		forward_back_spd *= retain;
		right_left_spd *= retain;
		up_down_spd *= retain;
		if (move_forward || wheel_forward)
			forward_back_spd += cam_spd * introduce;
		if (move_backward || wheel_backward)
			forward_back_spd -= cam_spd * introduce;
		forward_back_spd += cam_spd * introduce * float(wheel_forward - wheel_backward);
		wheel_forward = wheel_backward = 0;
		if (move_left)
			right_left_spd -= cam_spd * introduce;
		if (move_right)
			right_left_spd += cam_spd * introduce;
		if (move_up)
			up_down_spd += cam_spd * introduce;
		if (move_down)
			up_down_spd -= cam_spd * introduce;
		if (mouseButtons == core::MouseButtons::MiddleButton || mouseButtons == (core::MouseButtons::LeftButton | core::MouseButtons::RightButton))
		{
			static float slide_spd = 100.f;
			right_left_spd += slide_spd * fDeltaX * cam_spd * introduce;
			up_down_spd -= slide_spd * fDeltaY * cam_spd * introduce;
		}

		math::Vector3 view_dir = camera->Orientation.Tz();
		view_dir.Normalize();

		pos -= forward_back_spd * time_step * view_dir;
		pos += right_left_spd * time_step * camera->Orientation.Tx();
		(y_vertical ? pos.y : pos.z) += up_down_spd * time_step;
		(y_vertical ? pos.y : pos.z) = std::clamp((y_vertical ? pos.y : pos.z), minAlt, maxAlt);

		vec3d _vertical;
		{
			// Local Euclidean to global spherical.
			vec3d position;
			position.x = pos.x;
			position.y = y_vertical ? pos.z : pos.y;
			position.z = y_vertical ? pos.y : pos.z;
			position.z += length(enginePose.position);

			float Lat_Rad = float(position.y / position.z);
			float Lon_Rad = float(position.x / position.z);

			position.x = position.z * cos(Lat_Rad) * cos(Lon_Rad);
			position.y = position.z * cos(Lat_Rad) * sin(Lon_Rad);
			position.z = position.z * sin(Lat_Rad);
			_vertical = normalize(position);

			position -= enginePose.position;
			position = enginePose.orientation.conjugate().RotateVector(position);
			_vertical = enginePose.orientation.conjugate().RotateVector(_vertical);

			pos.x = (float)position.x;
			y_vertical ? pos.z : pos.y = (float)position.y;
			y_vertical ? pos.y : pos.z = (float)position.z;
		}
		camera->SetPosition(pos.Values);

		static float x_rotate = 0.f;
		static float y_rotate = 0.f;
		static float z_rotate = 0.f;
		x_rotate *= retain;
		y_rotate *= retain;
		z_rotate *= retain;
		float tilt = y_vertical ? asin(camera->Orientation.Tx().y) : asin(camera->Orientation.Tx().z);
		if (mouseButtons == core::MouseButtons::RightButton)
		{
			if (!alt_down)
			{
				if (cameraMode == FLYING)
					y_rotate -= fDeltaY * introduce;
				else
					y_rotate += fDeltaY * introduce;
				if (cameraMode == FLYING)
				{
					static float rr = 0.5f;
					z_rotate -= fDeltaX * introduce * rr;
				}
				else
				{
					static float xx = 1.5f;
					x_rotate += fDeltaX * introduce * xx;
				}
			}
		}
		if (mouseButtons == core::MouseButtons::MiddleButton)
		{
			// z_rotate-=fDeltaX*introduce;
		}
		static float cc = 0.01f;
		if (cameraMode == FLYING)
		{
			x_rotate -= tilt * cc * introduce;
		}
		fDeltaX = 0.f;
		fDeltaY = 0.f;

		math::Vector3 vertical(0.0f, 0.0f, 1.0f);
		if (y_vertical)
			vertical.Define(vertical.x, vertical.z, vertical.y);

		static float sr = 0.1f;
		math::Vector3 del = vertical * (x_rotate + sr * step_rotate_x) * (-1.f);
		step_rotate_x = 0;
		math::Vector3 dir = del;
		dir.Normalize();
		camera->Rotate(del.Magnitude(), dir.Values);

		del = camera->Orientation.Tx() * (y_rotate + sr * step_rotate_y) * (-1.f);
		step_rotate_y = 0;
		dir = del;
		dir.Normalize();
		camera->Rotate(del.Magnitude(), dir.Values);

		del = math::Vector3(0.f, 0.f, z_rotate);
		camera->LocalRotate(del.Values);

		static float correct_tilt = 0.005f;
		dir = camera->Orientation.Tz();
		dir.Normalize();
		if (!alt_down)
			camera->Rotate(-correct_tilt * tilt, view_dir.Values);
	}
	if(cameraMode==CENTRED)
	{
		math::Vector3 pos=camera->GetPosition();
		math::Vector3 dir;
		camera->Orientation.LocalToGlobalDirection(dir,offset_camspace);
		pos=centre;
		pos+=dir;
		camera->SetPosition(pos.Values);
		dir.Normalize();
		camera->LookInDirection((-dir).Values);
	}
	lastTimeStep = time_step;
}

void MouseHandler::KeyboardProc(KeyboardModifiers modifiers, bool bKeyDown)
{
	switch (modifiers)
	{
	case KeyboardModifiers::NoModifier:
		break;
	case KeyboardModifiers::Shift:
		shift_down = bKeyDown;
		break;
	case KeyboardModifiers::Alt:
		alt_down = bKeyDown;
		break;
	case KeyboardModifiers::Ctrl:
		ctrl_down = bKeyDown;
		break;
	default:
		break;
	}
}

void MouseHandler::KeyboardProc(CameraMovements movements, bool bKeyDown)
{
	switch (movements)
	{
	case CameraMovements::None:
	{
		break;
	}
	case CameraMovements::Forward:
	{
		if (bKeyDown)
		{
			move_forward = true;
			move_backward = false;
		}
		else
		{
			move_forward = false;
		}
		break;
	}
	case CameraMovements::Backward:
	{
		if (bKeyDown)
		{
			move_backward = true;
			move_forward = false;
		}
		else
		{
			move_backward = false;
		}
		break;
	}
	case CameraMovements::Left:
	{
		if (bKeyDown)
		{
			move_left = true;
			move_right = false;
		}
		else
		{
			move_left = false;
		}
		break;
	}
	case CameraMovements::Right:
	{
		if (bKeyDown)
		{
			move_right = true;
			move_left = false;
		}
		else
		{
			move_right = false;
		}
		break;
	}
	case CameraMovements::Up:
	{
		if (bKeyDown)
		{
			move_up = true;
			move_down = false;
		}
		else
		{
			move_up = false;
		}
		break;
	}
	case CameraMovements::Down:
	{	
		if (bKeyDown)
		{
			move_down = true;
			move_up = false;
		}
		else
		{
			move_down = false;
		}
		break;
	}
	case CameraMovements::StepRotateXPos:
	{
		if (bKeyDown)
			step_rotate_x = 1;
		else
			step_rotate_x = 0;

		break;
	}
	case CameraMovements::StepRotateXNeg:
	{
		if (bKeyDown)
			step_rotate_x = -1;
		else
			step_rotate_x = 0;
		break;
	}
	case CameraMovements::StepRotateYPos:
	{
		if (bKeyDown)
			step_rotate_y = 1;
		else
			step_rotate_y = 0;
		break;
	}
	case CameraMovements::StepRotateYNeg:
	{
		if (bKeyDown)
			step_rotate_y = -1;
		else
			step_rotate_y = 0;
		break;
	}
	default:
		break;
	}

	if (updateViews)
		updateViews();
}
