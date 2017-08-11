#version 450
#pragma shader_stage( vertex )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 0 ) uniform UBO {
	vec4 rpMVPmatrixX;
	vec4 rpMVPmatrixY;
	vec4 rpMVPmatrixZ;
	vec4 rpMVPmatrixW;
	vec4 rpTexGen0S;
	vec4 rpTexGen0T;
	vec4 rpTexGen0Q;
	vec4 rpTexGen1S;
};

layout( location = 0 ) in vec3 in_Position;

layout( location = 0 ) out vec4 out_TexCoord0;
layout( location = 1 ) out vec2 out_TexCoord1;

void main() {
	vec4 position = vec4( in_Position, 1.0 );
	gl_Position.x = dot( position, rpMVPmatrixX );
	gl_Position.y = dot( position, rpMVPmatrixY );
	gl_Position.z = dot( position, rpMVPmatrixZ );
	gl_Position.w = dot( position, rpMVPmatrixW );
	
	out_TexCoord0.x = dot( position, rpTexGen0S );
	out_TexCoord0.y = dot( position, rpTexGen0T );
	out_TexCoord0.z = 0.0;
	out_TexCoord0.w = dot( position, rpTexGen0Q );
	
	out_TexCoord1.x = dot( position, rpTexGen1S );
	out_TexCoord1.y = 0.5;
}