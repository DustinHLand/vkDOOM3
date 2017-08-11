#version 450
#pragma shader_stage( fragment )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 2 ) uniform UBO {
	vec4 rpWindowCoord;
};
layout( binding = 3 ) uniform sampler2D samp0;
layout( binding = 4 ) uniform sampler2D samp1;
layout( binding = 5 ) uniform sampler2D samp2;

layout( location = 0 ) in vec4 in_TexCoord0;
layout( location = 1 ) in vec4 in_TexCoord1;
layout( location = 2 ) in vec4 in_TexCoord2;

layout( location = 0 ) out vec4 out_Color;

void clip( vec4 v ) { 
	if ( any( lessThan( v, vec4( 0.0 ) ) ) ) { 
		discard; 
	} 
}

vec2 saturate( vec2 v ) { 
	return clamp( v, 0.0, 1.0 ); 
}

vec2 vposToScreenPosTexCoord( vec2 vpos ) { 
	return vpos.xy * rpWindowCoord.xy; 
}

void main() {
	vec4 mask = texture( samp2, in_TexCoord0.xy );
	mask.xy -= 0.01;
	clip( mask );
	vec4 bumpMap = ( texture( samp1, in_TexCoord1.xy ) * 2.0 ) - 1.0;
	vec2 localNormal = bumpMap.wy;
	localNormal *= mask.xy ;
	vec2 screenTexCoord = vposToScreenPosTexCoord( gl_FragCoord.xy );
	screenTexCoord += ( localNormal * in_TexCoord2.xy );
	screenTexCoord = saturate( screenTexCoord );
	out_Color = texture( samp0, screenTexCoord );
}