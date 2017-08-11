#version 450
#pragma shader_stage( vertex )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 0 ) uniform UBO {
	vec4 rpMVPmatrixX;
	vec4 rpMVPmatrixY;
	vec4 rpMVPmatrixZ;
	vec4 rpMVPmatrixW;
	vec4 rpUser0;
	vec4 rpUser1;
	vec4 rpUser2;
};

layout( location = 0 ) in vec3 in_Position;
layout( location = 1 ) in vec2 in_TexCoord;

layout( location = 0 ) out vec2 out_TexCoord0;
layout( location = 1 ) out vec2 out_TexCoord1;
layout( location = 2 ) out vec2 out_TexCoord2;
layout( location = 3 ) out vec2 out_TexCoord3;
layout( location = 4 ) out vec2 out_TexCoord4;

float dot4( vec2 a, vec4 b ) { return dot( vec4( a, 0, 1 ), b ); }

vec2 CenterScale( vec2 inTC, vec2 centerScale ) {
	float scaleX = centerScale.x;
	float scaleY = centerScale.y;
	vec4 tc0 = vec4( scaleX, 0, 0, 0.5 - ( 0.5 * scaleX ) );
	vec4 tc1 = vec4( 0, scaleY, 0, 0.5 - ( 0.5 * scaleY ) );
	vec2 finalTC;
	finalTC.x = dot4( inTC, tc0 );
	finalTC.y = dot4( inTC, tc1 );
	return finalTC;
}

vec2 Rotate2D( vec2 inTC, vec2 cs ) {
	float sinValue = cs.y;
	float cosValue = cs.x;
	vec4 tc0 = vec4( cosValue, -sinValue, 0, ( -0.5 * cosValue ) + ( 0.5 * sinValue ) + 0.5 );
	vec4 tc1 = vec4( sinValue, cosValue, 0, ( -0.5 * sinValue ) + ( -0.5 * cosValue ) + 0.5 );
	vec2 finalTC;
	finalTC.x = dot4( inTC, tc0 );
	finalTC.y = dot4( inTC, tc1 );
	return finalTC;
}

void main() {
	vec4 position = vec4( in_Position, 1.0 );
	gl_Position.x = dot( position, rpMVPmatrixX );
	gl_Position.y = dot( position, rpMVPmatrixY );
	gl_Position.z = dot( position, rpMVPmatrixZ );
	gl_Position.w = dot( position, rpMVPmatrixW );
	
	vec4 centerScaleTex = rpUser0;
	vec4 rotateTex = rpUser1;
	vec2 tc0 = CenterScale( in_TexCoord, centerScaleTex.xy );
	
	out_TexCoord0 = Rotate2D( tc0, rotateTex.xy );
	out_TexCoord1 = Rotate2D( tc0, vec2 ( rotateTex.z, -rotateTex.w ) );
	out_TexCoord2 = Rotate2D( tc0, rotateTex.zw );
	out_TexCoord3 = in_TexCoord;
	vec4 colorFactor = rpUser2;
	out_TexCoord4 = colorFactor.xx;
}