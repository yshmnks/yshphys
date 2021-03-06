#version 400

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_diffuse;
layout (location = 3) in vec3 in_specular;

uniform mat4 gProjection;
uniform mat4 gView;
uniform mat4 gModel;

out vec3 ex_vertPosition;
out vec3 ex_vertNormal;
out vec3 ex_vertDiffuse;
out vec3 ex_vertSpecular;

void main(void)
{
    gl_Position = gProjection * gView * gModel * vec4(in_position, 1.0f);
	ex_vertPosition = (gModel * vec4(in_position, 1.0f)).xyz;
	ex_vertNormal = mat3(gModel) * in_normal;
	ex_vertDiffuse = in_diffuse;
	ex_vertSpecular = in_specular;
}