#include "MouseHandler.h"

using namespace platform;
using namespace core;
using namespace crossplatform;

MouseHandler::MouseHandler()
	:step_rotate_x(0)
	,step_rotate_y(0)
	,cameraMode(LOOKAROUND)
	,CameraDamping(1e5f)
	,minAlt(0.f)
	,maxAlt(10000.f)
	,fov(40.f)
	,speed_factor(100.f)
	,y_vertical(false)
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
	camera=new platform::crossplatform::Camera();
	vec4 cameraPos(0.f,0,1500.f,0);
	camera->SetPosition(cameraPos);
	vec4 lookAtPos(-7.07f,cameraPos.y+0.f,7.07f,0);
	if(!y_vertical)
	{
		std::swap(lookAtPos.y,lookAtPos.z);
		std::swap(cameraPos.y,cameraPos.z);
	}
	camera->LookInDirection(lookAtPos-cameraPos);
	camera->SetHorizontalFieldOfViewDegrees(fov);
}

MouseHandler::~MouseHandler()
{
	delete camera;
}

void MouseHandler::setSpeed(float s)
{
	speed_factor=s;
}

void MouseHandler::setCentre(const float *c)
{
	centre[0]=c[0];
	centre[1]=c[1];
	centre[2]=c[2];
	if(y_vertical)
		std::swap(centre[1],centre[2]);
}

void MouseHandler::setCameraMode(CameraMode c)
{
	cameraMode=c;
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

void MouseHandler::getMousePosition(int &x,int &y) const
{
	x=MouseX;
	y=MouseY;
}

bool MouseHandler::getLeftButton() const
{
	return (mouseButtons==core::LeftButton);
}

bool MouseHandler::getRightButton() const
{
	return (mouseButtons==core::RightButton);
}

bool MouseHandler::getMiddleButton() const
{
	return (mouseButtons==core::MiddleButton);
}

void MouseHandler::mouseWheel(int delta,int modifiers)
{
	BaseMouseHandler::mouseWheel(delta,modifiers);
	static float min_deg=5.0f;
	if(delta<0&&fov<180.f/1.1f)
		fov*=1.1f;
	else if(delta>0&&fov>min_deg*1.1f)
		fov/=1.1f;
	camera->SetHorizontalFieldOfViewDegrees(fov);
	camera->SetVerticalFieldOfViewDegrees(0);
	if (updateViews)
		updateViews();
}

platform::crossplatform::Camera *MouseHandler::GetCamera()
{
	return camera;
}

void MouseHandler::setFov(float f)
{
	if(f<0.1f)
		f=0.1f;
	if(f>100.f)
		f=100.f;
	fov=f;
}

float MouseHandler::GetSpeed() const
{
	return sqrt(up_down_spd*up_down_spd+forward_back_spd*forward_back_spd+right_left_spd*right_left_spd);
}
void MouseHandler::Update(float time_step)
{
	camera->SetHorizontalFieldOfViewDegrees(fov);
	camera->SetVerticalFieldOfViewDegrees(0);
	platform::math::Vector3 offset_camspace;
	if(cameraMode==CENTRED)
	{
		platform::math::Vector3 pos=camera->GetPosition();
		platform::math::Vector3 dir=pos;
		dir-=centre;
		camera->Orientation.GlobalToLocalDirection(offset_camspace,dir);
	}
	{
		float cam_spd=speed_factor*(shift_down?100.f:1.f);
		platform::math::Vector3 pos=camera->GetPosition();

		float retain=1.f/(1.f+CameraDamping*time_step);
		float introduce=1.f-retain;

		forward_back_spd*=retain;
		right_left_spd*=retain;
		up_down_spd*=retain;
		if(move_forward)
			forward_back_spd+=cam_spd*introduce;
		if(move_backward)
			forward_back_spd-=cam_spd*introduce;
		if(move_left)
			right_left_spd-=cam_spd*introduce;
		if(move_right)
			right_left_spd+=cam_spd*introduce;
		if(move_up)
			up_down_spd+=cam_spd*introduce;
		if(move_down)
			up_down_spd-=cam_spd*introduce;
		if((mouseButtons&core::MiddleButton)||mouseButtons==(core::LeftButton|core::RightButton))
		{
			static float slide_spd=100.f;
			right_left_spd	+=slide_spd*fDeltaX*cam_spd*introduce;
			up_down_spd		-=slide_spd*fDeltaY*cam_spd*introduce;
		}

		if(y_vertical)
			pos+=forward_back_spd*time_step*camera->Orientation.Tz();
		else
			pos-=forward_back_spd*time_step*camera->Orientation.Tz();
		pos+=right_left_spd*time_step*camera->Orientation.Tx();
		if(y_vertical)
			pos.y+=up_down_spd*time_step;
		else
			pos.z+=up_down_spd*time_step;

		if((y_vertical?pos.y:pos.z)<minAlt)
			(y_vertical?pos.y:pos.z)=minAlt;
		if((y_vertical?pos.y:pos.z)>maxAlt)
			(y_vertical?pos.y:pos.z)=maxAlt;

		camera->SetPosition(pos);

		static float x_rotate=0.f;
		static float y_rotate=0.f;
		static float z_rotate=0.f;
		x_rotate*=retain;
		y_rotate*=retain;
		z_rotate*=retain;
		float tilt=0;
		if(y_vertical)
			tilt=asin(camera->Orientation.Tx().y);
		else
			tilt=asin(camera->Orientation.Tx().z);
		if(mouseButtons==core::RightButton)
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
		if(mouseButtons==core::MiddleButton)
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

		platform::math::Vector3 vertical(0,0,-1.f);
		if(y_vertical)
			vertical.Define(0,1.f,0);
		static float sr=0.1f;
		platform::math::Vector3 del=vertical*(x_rotate+sr*step_rotate_x);
		step_rotate_x=0;
		platform::math::Vector3 dir=del;
		dir.Normalize();
		camera->Rotate(del.Magnitude(),dir);

		del=camera->Orientation.Tx()*(y_rotate+sr*step_rotate_y)*(-1.f);
		step_rotate_y=0;
		dir=del;
		dir.Normalize();
		camera->Rotate(del.Magnitude(),dir);

		del=platform::math::Vector3(0.f,0.f,z_rotate);
		camera->LocalRotate(del);

		static float correct_tilt=0.005f;
		dir=camera->Orientation.Tz();
		dir.Normalize();
		if(!alt_down)
			camera->Rotate(-correct_tilt*tilt,dir);

	}
	if(cameraMode==CENTRED)
	{
		platform::math::Vector3 pos=camera->GetPosition();
		platform::math::Vector3 dir;
		camera->Orientation.LocalToGlobalDirection(dir,offset_camspace);
		pos=centre;
		pos+=dir;
		camera->SetPosition(pos);
		dir.Normalize();
		camera->LookInDirection(-dir);
	}
}


void MouseHandler::KeyboardProc(unsigned int nChar, bool bKeyDown, bool bAltDown)
{
	if(bKeyDown)
	{
		if(nChar==0x01000020||nChar==16)
		{
			shift_down=true;
		}
		if(nChar==18)
		{
			alt_down=true;
		}
		if(nChar==0x01000016||nChar==0x21)
		{
			move_up=true;
			move_down=false;
		}
		if(nChar==0x01000017||nChar==0x22)
		{
			move_down=true;
			move_up=false;
		}
		if(nChar=='w'||nChar=='W')
		{
			move_forward=true;
			move_backward=false;
		}
		if(nChar=='s'||nChar=='S')
		{
			move_backward=true;
			move_forward=false;
		}
		if(nChar=='a'||nChar=='A')
		{
			move_left=true;
			move_right=false;
		}
		if(nChar=='d'||nChar=='D')
		{
			move_right=true;
			move_left=false;
		}
		if (nChar == '5' )
		{
			return;
		}
		if(nChar==37) // arrow left
		{
			return;
		}
		if(nChar==39) // arrow right
		{
			return;
		}
		if(nChar==38) // arrow up
		{
			return;
		}
		if(nChar==40) // arrow down
		{
			return;
		}
		if (nChar == 32) // space
		{
			return;
		}
	}
	else
	{
		if(nChar==18)
		{
			alt_down=false;
		}
		if(nChar==0x01000020||nChar==16)
		{
			shift_down=false;
		}
		if(nChar==0x01000016||nChar==0x21)
		{
			move_up=false;
		}
		if(nChar==0x01000017||nChar==0x22)
		{
			move_down=false;
		}
		if(nChar=='w'||nChar=='W')
		{
			move_forward=false;
		}
		if(nChar=='s'||nChar=='S')
		{
			move_backward=false;
		}
		if(nChar=='a'||nChar=='A')
		{
			move_left=false;
		}
		if(nChar=='d'||nChar=='D')
		{
			move_right=false;
		}
		if(nChar==37) // arrow left
		{
			step_rotate_x=-1;
		}
		if(nChar==39) // arrow right
		{
			step_rotate_x=1;
		}
		if(nChar==38) // arrow up
		{
			step_rotate_y=1;
		}
		if(nChar==40) // arrow down
		{
			step_rotate_y=-1;
		}
	}
	if(updateViews)
		updateViews();
}
