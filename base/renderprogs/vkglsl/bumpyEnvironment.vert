#version 450
#pragma shader_stage( vertex )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 0 ) uniform UBO { 
	vec4 rpLocalViewOrigin;
	vec4 rpColor;
	vec4 rpMVPmatrixX;
	vec4 rpMVPmatrixY;
	vec4 rpMVPmatrixZ;
	vec4 rpMVPmatrixW;
	vec4 rpModelMatrixX;
	vec4 rpModelMatrixY;
	vec4 rpModelMatrixZ;
};

layout( location = 0 ) in vec3 in_Position;
layout( location = 1 ) in vec2 in_TexCoord;
layout( location = 2 ) in vec4 in_Normal;
layout( location = 3 ) in vec4 in_Tangent;

layout( location = 0 ) out vec2 out_TexCoord0;
layout( location = 1 ) out vec3 out_TexCoord1;
layout( location = 2 ) out vec3 out_TexCoord2;
layout( location = 3 ) out vec3 out_TexCoord3;
layout( location = 4 ) out vec3 out_TexCoord4;
layout( location = 5 ) out vec4 out_Color;

float dot3( vec3 a, vec4 b ) { return dot( a, b.xyz ); }
float dot3( vec4 a, vec4 b ) { return dot( a.xyz, b.xyz ); }

void main() {
	vec4 normal = in_Normal * 2.0 - 1.0;
	vec4 tangent = in_Tangent * 2.0 - 1.0;
	vec3 binormal = cross( normal.xyz, tangent.xyz ) * tangent.w;
	
	vec4 position = vec4( in_Position, 1.0 );
	gl_Position.x = dot( position, rpMVPmatrixX );
	gl_Position.y = dot( position, rpMVPmatrixY );
	gl_Position.z = dot( position, rpMVPmatrixZ );
	gl_Position.w = dot( position, rpMVPmatrixW );
	
	out_TexCoord0 = in_TexCoord.xy;
	
	vec4 toEye = rpLocalViewOrigin - position;
	out_TexCoord1.x = dot3( toEye, rpModelMatrixX );
	out_TexCoord1.y = dot3( toEye, rpModelMatrixY );
	out_TexCoord1.z = dot3( toEye, rpModelMatrixZ );
	
	out_TexCoord2.x = dot3( tangent, rpModelMatrixX );
	out_TexCoord3.x = dot3( tangent, rpModelMatrixY );
	out_TexCoord4.x = dot3( tangent, rpModelMatrixZ );
	
	out_TexCoord2.y = dot3( binormal, rpModelMatrixX );
	out_TexCoord3.y = dot3( binormal, rpModelMatrixY );
	out_TexCoord4.y = dot3( binormal, rpModelMatrixZ );
	
	out_TexCoord2.z = dot3( normal, rpModelMatrixX );
	out_TexCoord3.z = dot3( normal, rpModelMatrixY );
	out_TexCoord4.z = dot3( normal, rpModelMatrixZ );
	out_Color = rpColor;
}