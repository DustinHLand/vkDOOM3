#version 450
#pragma shader_stage( fragment )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 2 ) uniform UBO {
	vec4 rpAlphaTest;
};
layout( binding = 3 ) uniform sampler2D samp0;

layout( location = 0 ) in vec2 in_TexCoord0;
layout( location = 1 ) in vec4 in_Color;

layout( location = 0 ) out vec4 out_Color;

void clip( float v ) { 
	if ( v < 0.0 ) { 
		discard; 
	}
}

void main() {
	vec4 color = texture( samp0, in_TexCoord0.xy ) * in_Color;
	clip( color.a - rpAlphaTest.x );
	out_Color = color;
}