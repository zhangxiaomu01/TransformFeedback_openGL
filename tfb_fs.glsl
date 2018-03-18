#version 430

layout(location = 1) uniform float time;

layout(location = 0) out vec4 fragcolor;           
 
in float age_out;

void main(void)
{  
	//Creat a disk by applying a smoothstep function to the radius
	float r = length(gl_PointCoord.st-vec2(1.0));
	float r1 = length(gl_PointCoord.st);
	float r2 = length(gl_PointCoord.st-vec2(1.0,0.0));
	float r3 = length(gl_PointCoord.st-vec2(0.0,1.0));
	float s = smoothstep(0.495, 0.5, r3)*smoothstep(0.495, 0.5, r2)*smoothstep(0.495, 0.5, r1)*smoothstep(0.495, 0.5, r);

	//mix 2 hot colors to get a fire look.
	//vec4 color0 = vec4(0.7, 0.4, 0.3, 0.7*s);
	//vec4 color1 = vec4(0.7, 0.1, 0.0, 0.3*s);
	//float redchannel = mix(0.3,0.7, abs(sin(time)));


	vec4 color0 = vec4(0.1, 0.4, 0.9, 0.9*s);
	vec4 color1 = vec4(0.7, 0.1, 0.0, 0.5*s);
	fragcolor = mix(color1, color0, age_out/500.0);
	//fragcolor = vec4(s,s,s,1.0f);

}




















