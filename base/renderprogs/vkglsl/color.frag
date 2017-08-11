#version 450
#pragma shader_stage( fragment )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 1 ) uniform UBO { 
	vec4 rpColor;
};

layout( location = 0 ) out vec4 out_Color;

void main() {
	out_Color = rpColor;
}