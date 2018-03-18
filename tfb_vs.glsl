#version 430            

layout(location = 0) uniform mat4 PVM;
layout(location = 1) uniform float time;
layout(location = 2) uniform float slider = 0.5;

layout(location = 0) in vec3 pos_attrib;
layout(location = 1) in vec3 vel_attrib;
layout(location = 2) in float age_attrib;

out vec3 pos_out; 
out vec3 vel_out; 
out float age_out;

vec3 vel(vec3 p);
float rand(vec2 co);

//Basic velocity field
vec3 v0(vec3 p);

mat3 PRotation(float theta);

void main(void)
{
	
	mat3 R;
	vec2 seed = vec2(float(gl_VertexID), time); //seed for the random number generator
	float theta = -0.005 * rand(seed.xy);

	//Draw current particles
	gl_Position = PVM* vec4(pos_attrib, 1.0);
	//gl_PointSize = mix(4.0, 24.0*slider, age_attrib/1000.0);

	gl_PointSize = mix(4.0, 24.0*slider * clamp(rand(seed.yx),0.2f,1.0f), age_attrib/1000.0);
	//Compute particle attributes for next frame
	vel_out = vel(pos_attrib);

	float k = smoothstep(0.6, 1.2, 1.8*length(pos_attrib.xz));

	if(length(pos_attrib.xz)>k)
	{
		R = PRotation(theta);
	}
	else
	{
		R = PRotation(-theta);
	}

	pos_out = R*pos_attrib;// + vel_out;// ;// + 0.1*vel_out;
	age_out = age_attrib - 1.0;

	//Reinitialize particles
	if(age_out <= 0.0 || length(pos_out) > 2.0f)
	{
		
		float offsetX, offsetY, offsetZ;

		
		//pos_out = pos_attrib*0.0003 + (vec3(rand(seed.xy), rand(seed.yx), rand(seed.xx)) - vec3(0.5));cos(length(vec2(pos_attrib.xz))) + sin(time)
		//pos_out = pos_attrib*0.0003 + (vec3(rand(seed.xy)*sin(time), 0.5f, (rand(seed.xx)) - vec3(0.5))*cos(time));cos(time)*rand(seed.xy)*length(vec2(sin(time),cos(time)))
		offsetY = mix(0.02,0.8f,length(pos_attrib.xz));

		pos_out = pos_attrib*0.0003 + vec3(3*length(vec2(sin(time),cos(time)))*rand(seed.yy)* sin(time*seed.x) ,
											(offsetY-0.5f),
											3*length(vec2(sin(time),cos(time)))*rand(seed.yy) * cos(time*seed.x)) ;
		
		age_out = 500.0 + 200.0*(rand(pos_attrib.xy+seed)-0.5);
	}
}

mat3 PRotation(float theta)
{
	
	mat3 R = mat3(cos(theta),0, -sin(theta),
					0, 1, 0, 
				 sin(theta), 0, cos(theta)); 
	return R;
}



//Implements a fractal sum to make our velocity field a little more interesting
vec3 vel(vec3 p)
{
	const int n = 6;
	vec3 octaves = vec3(0.0);
	float scale = 1.0;
	//for(int i=0; i<n; i++)
	//{
	//	octaves = octaves + v0(scale*p)/scale;
	//	scale*= 2.0;
	//}
	const float theta = -0.01;
	mat2 R = mat2(cos(theta), sin(theta), -sin(theta),cos(theta)); 
	vec3 direction =vec3(p.x *sin(time), 0.0f, p.z* cos(time));
	octaves = vec3(R*vec2(p.x, p.z), 0.0f);

	return octaves;
}

vec3 v0(vec3 p)
{
	return 15.0*vec3(sin(p.y*10.0+time-10.0), -sin(p.x*10.0+9.0*time+10.0), +cos(2.4*p.z+2.0*time));
}

// a hacky random numver generator
float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}
