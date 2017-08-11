#version 450
#pragma shader_stage( fragment )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 1 ) uniform UBO {
	vec4 rpDiffuseModifier;
	vec4 rpSpecularModifier;
};
layout( binding = 2 ) uniform sampler2D samp0;
layout( binding = 3 ) uniform sampler2D samp1;
layout( binding = 4 ) uniform sampler2D samp2;
layout( binding = 5 ) uniform sampler2D samp3;
layout( binding = 6 ) uniform sampler2D samp4;

layout( location = 0 ) in vec4 in_TexCoord0;
layout( location = 1 ) in vec4 in_TexCoord1;
layout( location = 2 ) in vec4 in_TexCoord2;
layout( location = 3 ) in vec4 in_TexCoord3;
layout( location = 4 ) in vec4 in_TexCoord4;
layout( location = 5 ) in vec4 in_TexCoord5;
layout( location = 6 ) in vec4 in_TexCoord6;
layout( location = 7 ) in vec4 in_Color;

layout( location = 0 ) out vec4 out_Color;

const vec4 matrixCoCg1YtoRGB1X = vec4( 1.0, -1.0, 0.0, 1.0 );
const vec4 matrixCoCg1YtoRGB1Y = vec4( 0.0, 1.0, -0.50196078, 1.0 );
const vec4 matrixCoCg1YtoRGB1Z = vec4( -1.0, -1.0, 1.00392156, 1.0 );
vec3 ConvertYCoCgToRGB( vec4 YCoCg ) {
	vec3 rgbColor;
	YCoCg.z = ( YCoCg.z * 31.875 ) + 1.0;
	YCoCg.z = 1.0 / YCoCg.z;
	YCoCg.xy *= YCoCg.z;
	rgbColor.x = dot( YCoCg , matrixCoCg1YtoRGB1X );
	rgbColor.y = dot( YCoCg , matrixCoCg1YtoRGB1Y );
	rgbColor.z = dot( YCoCg , matrixCoCg1YtoRGB1Z );
	return rgbColor ;
}

void main() {
	vec4 bumpMap = texture( samp0, in_TexCoord1.xy );
	vec4 lightFalloff = textureProj( samp1, in_TexCoord2.xyw );
	vec4 lightProj = textureProj( samp2, in_TexCoord3.xyw );
	vec4 YCoCG = texture( samp3, in_TexCoord4.xy );
	vec4 specMap = texture( samp4, in_TexCoord5.xy );
	vec3 lightVector = normalize( in_TexCoord0.xyz );
	vec3 diffuseMap = ConvertYCoCgToRGB( YCoCG );
	vec3 localNormal;
	localNormal.xy = bumpMap.wy - 0.5;
	localNormal.z = sqrt( abs( dot( localNormal.xy, localNormal.xy ) - 0.25 ) );
	localNormal = normalize( localNormal );
	float specularPower = 10.0;
	float hDotN = dot( normalize( in_TexCoord6.xyz ), localNormal );
	vec3 specularContribution = vec3( pow( hDotN, specularPower ) );
	vec3 diffuseColor = diffuseMap * rpDiffuseModifier.xyz;
	vec3 specularColor = specMap.xyz * specularContribution * rpSpecularModifier.xyz;
	vec3 lightColor = dot( lightVector, localNormal ) * lightProj.xyz * lightFalloff.xyz;
	out_Color.xyz = ( diffuseColor + specularColor ) * lightColor * in_Color.xyz;
	out_Color.w = 1.0;
}