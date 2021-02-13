#version 450

layout (location = 0) in VS_OUT {
	vec4 position;
} fs_in;

layout (location = 0) out vec2 frag_color;

void main() {
	// we multiply the depth by .1 and add a bias
	float depth = -fs_in.position.z * 0.1 - 0.0005;
	
	float moment1 = depth;
	float moment2 = depth * depth;
	
	// Adjusting moments (this is sort of bias per pixel) using partial derivative
	float dx = dFdx(depth);
	float dy = dFdy(depth);
	moment2 += 0.25*(dx*dx+dy*dy);
		
	frag_color = vec2(moment1, moment2);
}