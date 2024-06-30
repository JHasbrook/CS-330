#version 330 core
out vec4 fragmentColor;

in vec3 fragmentPosition;
in vec3 fragmentVertexNormal;
in vec2 fragmentTextureCoordinate;
in vec4 FragPosLightSpace;

struct Material {
    vec3 diffuseColor;
    vec3 specularColor;
    float shininess;
    vec3 emissiveColor; // Include emissive color
};

struct DirectionalLight {
    vec3 direction;
    
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    bool bActive;
};

struct PointLight {
    vec3 position;
    
    float constant;
    float linear;
    float quadratic;
    
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    bool bActive;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
  
    float constant;
    float linear;
    float quadratic;
  
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;       

    bool bActive;
};

#define TOTAL_POINT_LIGHTS 4

uniform bool bUseTexture = false;
uniform bool bUseLighting = false;
uniform vec4 objectColor = vec4(1.0f);
uniform vec3 viewPosition;
uniform DirectionalLight directionalLight;
uniform PointLight pointLights[TOTAL_POINT_LIGHTS];
uniform SpotLight spotLight;
uniform Material material;
uniform sampler2D objectTexture;
uniform sampler2D shadowMap;
uniform vec2 UVscale = vec2(1.0f, 1.0f);
uniform float tintIntensity = 0.0; // Default to no tint
uniform vec3 tintColor = vec3(0.0, 0.0, 0.0); // Tint color (default to black)

// function prototypes
vec3 CalcDirectionalLight(DirectionalLight light, vec3 normal, vec3 viewDir);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir, out vec3 projCoords);

void main()
{    
    vec4 finalColor;
    vec3 projCoords;  // Declare projCoords in main function scope
    float shadow = 0.0;  // Declare and initialize shadow variable

    vec3 lightDir = normalize(directionalLight.direction); // Assuming directionalLight is your main light source

    if (bUseLighting == true)
    {
        vec3 phongResult = vec3(0.0f);
        // properties
        vec3 norm = normalize(fragmentVertexNormal);
        vec3 viewDir = normalize(viewPosition - fragmentPosition);
    
        // phase 1: directional lighting
        if (directionalLight.bActive == true)
        {
            phongResult = CalcDirectionalLight(directionalLight, norm, viewDir);
        }
        // phase 2: point lights
        for (int i = 0; i < TOTAL_POINT_LIGHTS; i++) {
            if (pointLights[i].bActive) {
                phongResult += CalcPointLight(pointLights[i], norm, fragmentPosition, viewDir);
            }
        }
        // phase 3: spot light
        if (spotLight.bActive == true)
        {
            phongResult += CalcSpotLight(spotLight, norm, fragmentPosition, viewDir);    
        }

        // Calculate shadow
        shadow = ShadowCalculation(FragPosLightSpace, fragmentVertexNormal, lightDir, projCoords);

        vec3 emissive = material.emissiveColor; // Get emissive color

        if (bUseTexture)
        {
            vec4 textureColor = texture(objectTexture, fragmentTextureCoordinate * UVscale);
            vec3 tintedTextureColor = mix(textureColor.rgb, tintColor, tintIntensity); // Apply tint
            finalColor = vec4(phongResult * tintedTextureColor * (1.0 - shadow) + emissive, textureColor.a);
        }
        else
        {
            finalColor = vec4(phongResult * objectColor.rgb * (1.0 - shadow) + emissive, objectColor.a);
        }
    }
    else
    {
        vec3 emissive = material.emissiveColor; // Get emissive color

        if (bUseTexture)
        {
            vec4 textureColor = texture(objectTexture, fragmentTextureCoordinate * UVscale);
            vec3 tintedTextureColor = mix(textureColor.rgb, tintColor, tintIntensity); // Apply tint
            finalColor = vec4(tintedTextureColor + emissive, textureColor.a);
        }
        else
        {
            finalColor = vec4(objectColor.rgb + emissive, objectColor.a);
        }

        shadow = 0.0;  // Set shadow to 0 if lighting is not used
    }

    fragmentColor = finalColor;
}

// calculates the color when using a directional light.
vec3 CalcDirectionalLight(DirectionalLight light, vec3 normal, vec3 viewDir)
{
    vec3 ambient = vec3(0.0f);
    vec3 diffuse = vec3(0.0f);
    vec3 specular = vec3(0.0f);

    vec3 lightDirection = normalize(-light.direction);
    // diffuse shading
    float diff = max(dot(normal, lightDirection), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDirection, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // combine results
    if (bUseTexture == true)
    {
        ambient = light.ambient * vec3(texture(objectTexture, fragmentTextureCoordinate));
        diffuse = light.diffuse * diff * material.diffuseColor * vec3(texture(objectTexture, fragmentTextureCoordinate));
    }
    else
    {
        ambient = light.ambient;
        diffuse = light.diffuse * diff * material.diffuseColor;
    }
    specular = light.specular * spec * material.specularColor;
    return (ambient + diffuse + specular);
}

// calculates the color when using a point light.
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 ambient = vec3(0.0f);
    vec3 diffuse = vec3(0.0f);
    vec3 specular = vec3(0.0f);

    if (light.bActive == true)
    {
        vec3 lightDir = normalize(light.position - fragPos);
        // diffuse shading
        float diff = max(dot(normal, lightDir), 0.0);
        // specular shading
        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
        // attenuation
        float distance = length(light.position - fragPos);
        float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
        // combine results
        if (bUseTexture == true)
        {
            ambient = light.ambient * vec3(texture(objectTexture, fragmentTextureCoordinate));
            diffuse = light.diffuse * diff * material.diffuseColor * vec3(texture(objectTexture, fragmentTextureCoordinate));
        }
        else
        {
            ambient = light.ambient;
            diffuse = light.diffuse * diff * material.diffuseColor;
        }
        specular = light.specular * spec * material.specularColor;
        ambient *= attenuation;
        diffuse *= attenuation;
        specular *= attenuation;
    }
    return (ambient + diffuse + specular);
}

// calculates the color when using a spot light.
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 ambient = vec3(0.0f);
    vec3 diffuse = vec3(0.0f);
    vec3 specular = vec3(0.0f);

    vec3 lightDir = normalize(light.position - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
    // spotlight intensity
    float theta = dot(lightDir, normalize(-light.direction)); 
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    // combine results
    if (bUseTexture == true)
    {
        ambient = light.ambient * vec3(texture(objectTexture, fragmentTextureCoordinate));
        diffuse = light.diffuse * diff * material.diffuseColor * vec3(texture(objectTexture, fragmentTextureCoordinate));
    }
    else
    {
        ambient = light.ambient;
        diffuse = light.diffuse * diff * material.diffuseColor;
    }
    specular = light.specular * spec * material.specularColor;
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
    return (ambient + diffuse + specular);
}

// Shadow calculation function
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir, out vec3 projCoords)
{
    projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0)
        return 0.0;

    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.005);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += projCoords.z - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return 0;
}
