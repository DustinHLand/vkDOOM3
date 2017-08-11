#version 450
#pragma shader_stage( fragment )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 2 ) uniform UBO {
	vec4 rpColor;
};
layout( binding = 3 ) uniform sampler2D samp0;
layout( binding = 4 ) uniform sampler2D samp1;

layout( location = 0 ) in vec2 in_TexCoord0;
layout( location = 1 ) in vec2 in_TexCoord1;

layout( location = 0 ) out vec4 out_Color;

void main() {
	out_Color = texture( samp0, in_TexCoord0.xy ) * texture( samp1, in_TexCoord1.xy ) * rpColor;
}