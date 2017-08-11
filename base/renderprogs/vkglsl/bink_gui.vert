#version 450
#pragma shader_stage( vertex )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 0 ) uniform UBO {
	vec4 rpMVPmatrixX;
	vec4 rpMVPmatrixY;
	vec4 rpMVPmatrixZ;
	vec4 rpMVPmatrixW;
};

layout( location = 0 ) in vec3 in_Position;
layout( location = 1 ) in vec2 in_TexCoord;
layout( location = 2 ) in vec4 in_Normal;
layout( location = 3 ) in vec4 in_Tangent;
layout( location = 4 ) in vec4 in_Color;
layout( location = 5 ) in vec4 in_Color2;

layout( location = 0 ) out vec2 out_TexCoord0;
layout( location = 1 ) out vec4 out_TexCoord1;
layout( location = 2 ) out vec4 out_Color;

void main() {
	vec4 position = vec4( in_Position, 1.0 );
	gl_Position.x = dot( position, rpMVPmatrixX );
	gl_Position.y = dot( position, rpMVPmatrixY );
	gl_Position.z = dot( position, rpMVPmatrixZ );
	gl_Position.w = dot( position, rpMVPmatrixW );
	
	out_TexCoord0.xy = in_TexCoord.xy;
	out_TexCoord1 = ( in_Color2 * 2 ) - 1;
	
	out_Color = in_Color;
}