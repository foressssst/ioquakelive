#version 450

layout(set = 0, binding = 0) buffer SSBO {
	int sampled;
};

layout(location = 0) out vec4 out_color;
layout(early_fragment_tests) in;

void main() {
	sampled = 1;
	discard;
}
