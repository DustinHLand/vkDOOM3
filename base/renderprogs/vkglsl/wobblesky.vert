#version 450
#pragma shader_stage( vertex )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 0 ) uniform UBO {
	vec4 rpLocalViewOrigin;
	vec4 rpVertexColorModulate;
	vec4 rpVertexColorAdd;
	vec4 rpMVPmatrixX;
	vec4 rpMVPmatrixY;
	vec4 rpMVPmatrixZ;
	vec4 rpMVPmatrixW;
	vec4 rpWobbleSkyX;
	vec4 rpWobbleSkyY;
	vec4 rpWobbleSkyZ;
};

layout( location = 0 ) in vec3 in_Position;
layout( location = 1 ) in vec2 in_TexCoord;
layout( location = 2 ) in vec4 in_Normal;
layout( location = 3 ) in vec4 in_Tangent;
layout( location = 4 ) in vec4 in_Color;

layout( location = 0 ) out vec3 out_TexCoord0;
layout( location = 1 ) out vec4 out_Color;

float dot3( vec3 a, vec4 b ) { return dot( a, b.xyz ); }

float dot4( vec2 a, vec4 b ) { return dot( vec4( a, 0, 1 ), b ); }

void main() {
	vec4 position = vec4( in_Position, 1.0 );
	gl_Position.x = dot( position, rpMVPmatrixX );
	gl_Position.y = dot( position, rpMVPmatrixY );
	gl_Position.z = dot( position, rpMVPmatrixZ );
	gl_Position.w = dot( position, rpMVPmatrixW );
	
	vec3 t0 = position.xyz - rpLocalViewOrigin.xyz;
	out_TexCoord0.x = dot3( t0, rpWobbleSkyX );
	out_TexCoord0.y = dot3( t0, rpWobbleSkyY );
	out_TexCoord0.z = dot3( t0, rpWobbleSkyZ );
	out_Color = ( in_Color * rpVertexColorModulate ) + rpVertexColorAdd;
}