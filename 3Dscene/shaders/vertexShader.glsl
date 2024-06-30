#version 330 core
layout (location = 0) in vec3 inVertexPosition;
layout (location = 1) in vec3 inVertexNormal;
layout (location = 2) in vec2 inTextureCoordinate;

out vec3 fragmentPosition;
out vec3 fragmentVertexNormal;
out vec2 fragmentTextureCoordinate;
out vec4 FragPosLightSpace;  // Output for shadow mapping

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;  // Uniform for light space matrix

void main()
{
    fragmentPosition = vec3(model * vec4(inVertexPosition, 1.0));
    gl_Position = projection * view * model * vec4(inVertexPosition, 1.0f);
    fragmentVertexNormal = inVertexNormal;
    fragmentTextureCoordinate = inTextureCoordinate;
    FragPosLightSpace = lightSpaceMatrix * vec4(fragmentPosition, 1.0);  // Compute light space position
}
