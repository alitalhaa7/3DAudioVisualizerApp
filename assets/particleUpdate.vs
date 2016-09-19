#version 150 core

uniform float Disturbance;
uniform vec3  DisturbancePosition;

in vec3   iPosition;
in vec3   iPPostion;
in vec3   iHome;
in float  iDamping;
in vec4   iColor;

out vec3  position;
out vec3  pposition;
out vec3  home;
out float damping;
out vec4  color;

const float dt2 = 1.0 / (60.0 * 60.0);

void main()
{
	position =  iPosition;
	pposition = iPPostion;
	damping =   iDamping;
	home =      iHome;
	color =     iColor;
	vec4 newcol=iColor.xyzw;
	// mouse interaction
	if( Disturbance > 0.0 ) {
		vec3 dir = position - DisturbancePosition;
		float d2 = length( dir );
		d2 *= d2;
		position += Disturbance * dir / d2;
	}

	vec3 vel = (position - pposition)*damping;
	pposition = position;
	vec3 acc = (home - position) * 10000.0f;
	color = vec4(newcol);

	position += vel + acc * dt2;
}