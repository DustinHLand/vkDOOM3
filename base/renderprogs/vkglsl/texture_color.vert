#version 450
#pragma shader_stage( vertex )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 0 ) uniform UBO {
	vec4 rpVertexColorModulate;
	vec4 rpVertexColorAdd;
	vec4 rpColor;
	vec4 rpMVPmatrixX;
	vec4 rpMVPmatrixY;
	vec4 rpMVPmatrixZ;
	vec4 rpMVPmatrixW;
	vec4 rpTextureMatrixS;
	vec4 rpTextureMatrixT;
	vec4 rpTexGen0S;
	vec4 rpTexGen0T;
	vec4 rpTexGen0Enabled;
};

layout( location = 0 ) in vec3 in_Position;
layout( location = 1 ) in vec2 in_TexCoord;
layout( location = 2 ) in vec4 in_Normal;
layout( location = 3 ) in vec4 in_Tangent;
layout( location = 4 ) in vec4 in_Color;
layout( location = 5 ) in vec4 in_Color2;

layout( location = 0 ) out vec2 out_TexCoord0;
layout( location = 1 ) out vec4 out_Color;

float dot4( vec2 a, vec4 b ) { return dot( vec4( a, 0, 1 ), b ); }

void main() {
	vec4 position = vec4( in_Position, 1.0 );
	gl_Position.x = dot( position, rpMVPmatrixX );
	gl_Position.y = dot( position, rpMVPmatrixY );
	gl_Position.z = dot( position, rpMVPmatrixZ );
	gl_Position.w = dot( position, rpMVPmatrixW );
	
	if ( rpTexGen0Enabled.x > 0.0 ) {
		out_TexCoord0.x = dot( position, rpTexGen0S );
		out_TexCoord0.y = dot( position, rpTexGen0T );
	} else {
		out_TexCoord0.x = dot4( in_TexCoord.xy, rpTextureMatrixS );
		out_TexCoord0.y = dot4( in_TexCoord.xy, rpTextureMatrixT );
	}
	vec4 vertexColor = ( in_Color * rpVertexColorModulate ) + rpVertexColorAdd;
	out_Color = vertexColor * rpColor;
}