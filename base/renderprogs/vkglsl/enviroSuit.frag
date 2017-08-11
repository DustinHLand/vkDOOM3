#version 450
#pragma shader_stage( fragment )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 1 ) uniform sampler2D samp0;
layout( binding = 2 ) uniform sampler2D samp1;

layout( location = 0 ) in vec2 in_TexCoord0;
layout( location = 1 ) in vec4 in_Color;

layout( location = 0 ) out vec4 out_Color;

void main() {
	vec2 screenTexCoord = in_TexCoord0;
	vec4 warpFactor = 1.0 - ( texture( samp1, screenTexCoord.xy ) * in_Color );
	screenTexCoord -= vec2( 0.5 , 0.5 );
	screenTexCoord *= warpFactor.xy;
	screenTexCoord += vec2( 0.5, 0.5 );
	out_Color = texture( samp0, screenTexCoord.xy );
}