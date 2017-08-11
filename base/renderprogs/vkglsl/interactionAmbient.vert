#version 450
#pragma shader_stage( vertex )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 0 ) uniform UBO {
	vec4 rpLocalLightOrigin;
	vec4 rpLocalViewOrigin;
	vec4 rpLightProjectionS;
	vec4 rpLightProjectionT;
	vec4 rpLightProjectionQ;
	vec4 rpLightFalloffS;
	vec4 rpBumpMatrixS;
	vec4 rpBumpMatrixT;
	vec4 rpDiffuseMatrixS;
	vec4 rpDiffuseMatrixT;
	vec4 rpSpecularMatrixS;
	vec4 rpSpecularMatrixT;
	vec4 rpVertexColorModulate;
	vec4 rpVertexColorAdd;
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

layout( location = 0 ) out vec4 out_TexCoord1;
layout( location = 1 ) out vec4 out_TexCoord2;
layout( location = 2 ) out vec4 out_TexCoord3;
layout( location = 3 ) out vec4 out_TexCoord4;
layout( location = 4 ) out vec4 out_TexCoord5;
layout( location = 5 ) out vec4 out_TexCoord6;
layout( location = 6 ) out vec4 out_Color;

float dot3( vec3 a, vec4 b ) { return dot( a, b.xyz ); }
float dot3( vec4 a, vec4 b ) { return dot( a.xyz, b.xyz ); }

float dot4( vec2 a, vec4 b ) { return dot( vec4( a, 0, 1 ), b ); }

void main() {
	vec4 normal = in_Normal * 2.0 - 1.0;
	vec4 tangent = in_Tangent * 2.0 - 1.0;
	vec3 binormal = cross( normal.xyz, tangent.xyz ) * tangent.w;
	
	vec4 position = vec4( in_Position, 1.0 );
	gl_Position.x = dot( position, rpMVPmatrixX );
	gl_Position.y = dot( position, rpMVPmatrixY );
	gl_Position.z = dot( position, rpMVPmatrixZ );
	gl_Position.w = dot( position, rpMVPmatrixW );
	
	vec4 defaultTexCoord = vec4( 0.0, 0.5, 0.0, 1.0 );
	vec4 toLight = rpLocalLightOrigin - position;
	out_TexCoord1 = defaultTexCoord;
	out_TexCoord1.x = dot4( in_TexCoord.xy, rpBumpMatrixS );
	out_TexCoord1.y = dot4( in_TexCoord.xy, rpBumpMatrixT );
	
	out_TexCoord2 = defaultTexCoord;
	out_TexCoord2.x = dot( position, rpLightFalloffS );
	
	out_TexCoord3.x = dot( position, rpLightProjectionS );
	out_TexCoord3.y = dot( position, rpLightProjectionT );
	out_TexCoord3.z = 0.0;
	out_TexCoord3.w = dot( position, rpLightProjectionQ );
	
	out_TexCoord4 = defaultTexCoord;
	out_TexCoord4.x = dot4( in_TexCoord.xy, rpDiffuseMatrixS );
	out_TexCoord4.y = dot4( in_TexCoord.xy, rpDiffuseMatrixT );
	
	out_TexCoord5 = defaultTexCoord;
	out_TexCoord5.x = dot4( in_TexCoord.xy, rpSpecularMatrixS );
	out_TexCoord5.y = dot4( in_TexCoord.xy, rpSpecularMatrixT );
	
	toLight = normalize ( toLight );
	
	vec4 toView = normalize( rpLocalViewOrigin - position );
	vec4 halfAngleVector = toLight + toView;
	out_TexCoord6.x = dot3( tangent, halfAngleVector );
	out_TexCoord6.y = dot3( binormal, halfAngleVector );
	out_TexCoord6.z = dot3( normal, halfAngleVector );
	out_TexCoord6.w = 1.0;
	
	out_Color = ( in_Color * rpVertexColorModulate ) + rpVertexColorAdd;
}