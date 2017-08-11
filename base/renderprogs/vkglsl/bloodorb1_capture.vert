#version 450
#pragma shader_stage( vertex )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 0 ) uniform UBO {
	vec4 rpMVPmatrixX;
	vec4 rpMVPmatrixY;
	vec4 rpMVPmatrixZ;
	vec4 rpMVPmatrixW;
	vec4 rpUser0;
};

layout( location = 0 ) in vec3 in_Position;
layout( location = 1 ) in vec2 in_TexCoord;

layout( location = 0 ) out vec2 out_TexCoord0;
layout( location = 1 ) out vec2 out_TexCoord1;

float dot4( vec2 a, vec4 b ) { return dot( vec4( a, 0, 1 ), b ); }

vec2 CenterScale( vec2 inTC , vec2 centerScale ) {
	float scaleX = centerScale.x;
	float scaleY = centerScale.y;
	vec4 tc0 = vec4( scaleX, 0, 0, 0.5 - ( 0.5 * scaleX ) );
	vec4 tc1 = vec4( 0, scaleY, 0, 0.5 - ( 0.5 * scaleY ) );
	vec2 finalTC;
	finalTC.x = dot4( inTC , tc0 );
	finalTC.y = dot4( inTC , tc1 );
	return finalTC;
}

void main() {
	vec4 position = vec4( in_Position, 1.0 );
	gl_Position.x = dot( position, rpMVPmatrixX );
	gl_Position.y = dot( position, rpMVPmatrixY );
	gl_Position.z = dot( position, rpMVPmatrixZ );
	gl_Position.w = dot( position, rpMVPmatrixW );
	
	vec4 centerScale = rpUser0;
	out_TexCoord0 = CenterScale( in_TexCoord, centerScale.xy );
	out_TexCoord1 = in_TexCoord;
}