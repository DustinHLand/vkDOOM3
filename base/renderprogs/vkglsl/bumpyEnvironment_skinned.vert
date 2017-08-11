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
layout( binding = 1 ) uniform MAT_UBO { vec4 matrices[ 408 ]; };

layout( location = 0 ) in vec3 in_Position;
layout( location = 1 ) in vec2 in_TexCoord;
layout( location = 2 ) in vec4 in_Normal;
layout( location = 3 ) in vec4 in_Tangent;
layout( location = 4 ) in vec4 in_Color;
layout( location = 5 ) in vec4 in_Color2;

layout( location = 0 ) out vec2 out_TexCoord0;
layout( location = 1 ) out vec3 out_TexCoord1;
layout( location = 2 ) out vec3 out_TexCoord2;
layout( location = 3 ) out vec3 out_TexCoord3;
layout( location = 4 ) out vec3 out_TexCoord4;
layout( location = 5 ) out vec4 out_Color;

float dot3( vec4 a, vec3 b ) { return dot( a.xyz, b ); }
float dot3( vec3 a, vec4 b ) { return dot( a, b.xyz ); }
float dot3( vec4 a, vec4 b ) { return dot( a.xyz, b.xyz ); }

void main() {
	vec4 vNormal = in_Normal * 2.0 - 1.0;
	vec4 vTangent = in_Tangent * 2.0 - 1.0;
	vec3 vBinormal = cross( vNormal.xyz, vTangent.xyz ) * vTangent.w;
	
	float w0 = in_Color2.x;
	float w1 = in_Color2.y;
	float w2 = in_Color2.z;
	float w3 = in_Color2.w;
	
	vec4 matX, matY, matZ;
	float joint = in_Color.x * 255.1 * 3;
	matX = matrices[ int( joint + 0 ) ] * w0;
	matY = matrices[ int( joint + 1 ) ] * w0;
	matZ = matrices[ int( joint + 2 ) ] * w0;
	joint = in_Color.y * 255.1 * 3;
	matX += matrices[ int( joint + 0 ) ] * w1;
	matY += matrices[ int( joint + 1 ) ] * w1;
	matZ += matrices[ int( joint + 2 ) ] * w1;
	joint = in_Color.z * 255.1 * 3;
	matX += matrices[ int( joint + 0 ) ] * w2;
	matY += matrices[ int( joint + 1 ) ] * w2;
	matZ += matrices[ int( joint + 2 ) ] * w2;
	joint = in_Color.w * 255.1 * 3;
	matX += matrices[ int( joint + 0 ) ] * w3;
	matY += matrices[ int( joint + 1 ) ] * w3;
	matZ += matrices[ int( joint + 2 ) ] * w3;
	
	vec3 normal;
	normal.x = dot3( matX, vNormal );
	normal.y = dot3( matY, vNormal );
	normal.z = dot3( matZ, vNormal );
	normal = normalize ( normal );
	
	vec3 tangent;
	tangent.x = dot3( matX, vTangent );
	tangent.y = dot3( matY, vTangent );
	tangent.z = dot3( matZ, vTangent );
	tangent = normalize( tangent );
	
	vec3 binormal;
	binormal.x = dot3( matX, vBinormal );
	binormal.y = dot3( matY, vBinormal );
	binormal.z = dot3( matZ, vBinormal );
	binormal = normalize( binormal );
	
	vec4 modelPosition;
	vec4 position = vec4( in_Position, 1.0 );
	modelPosition.x = dot( matX, position );
	modelPosition.y = dot( matY, position );
	modelPosition.z = dot( matZ, position );
	modelPosition.w = 1.0;
	
	gl_Position.x = dot( modelPosition, rpMVPmatrixX );
	gl_Position.y = dot( modelPosition, rpMVPmatrixY );
	gl_Position.z = dot( modelPosition, rpMVPmatrixZ );
	gl_Position.w = dot( modelPosition, rpMVPmatrixW );
	out_TexCoord0 = in_TexCoord.xy;
	
	vec4 toEye = rpLocalViewOrigin - modelPosition;
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