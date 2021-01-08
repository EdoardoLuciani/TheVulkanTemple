#version 450

layout (location = 0) in VS_OUT {
	vec4 position;
} fs_in;

layout (location = 0) out vec2 frag_color;

void main() {
    float depth = fs_in.position.z / fs_in.position.w;
	//float depth = length(fs_in.position);
	depth = depth * 0.5 + 0.5;			//Don't forget to move away from unit cube ([-1,1]) to [0,1] coordinate system
	
	float moment1 = depth;
	float moment2 = depth * depth;
	
	// Adjusting moments (this is sort of bias per pixel) using partial derivative
	float dx = dFdx(depth);
	float dy = dFdy(depth);
	moment2 += 0.25*(dx*dx+dy*dy);
		
	frag_color = vec2(moment1, moment2);
}