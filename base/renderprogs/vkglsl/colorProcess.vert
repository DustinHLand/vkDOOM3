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
};

layout( location = 0 ) in vec3 in_Position;
layout( location = 1 ) in vec2 in_TexCoord;

layout( location = 0 ) out vec3 out_TexCoord0;
layout( location = 1 ) out vec4 out_Color;

void main() {
	vec4 position = vec4( in_Position, 1.0 );
	gl_Position.x = dot( position, rpMVPmatrixX );
	gl_Position.y = dot( position, rpMVPmatrixY );
	gl_Position.z = dot( position, rpMVPmatrixZ );
	gl_Position.w = dot( position, rpMVPmatrixW );
	
	out_Color = rpUser1;
	out_TexCoord0.x = in_TexCoord.x;
	out_TexCoord0.y = 1.0 - in_TexCoord.y;
	out_TexCoord0.z = rpUser0.x;
}