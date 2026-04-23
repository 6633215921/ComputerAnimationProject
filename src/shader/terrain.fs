#version 330 core
out vec4 FragColor;

in float Height;
in vec2 vTexCoord;
in vec3 FragPos;
in vec3 vNormal;

uniform sampler2D uTextureWater;
uniform sampler2D uTextureSand;
uniform sampler2D uTextureGrass;
uniform sampler2D uTextureRock;
uniform sampler2D uTextureSnow;

uniform vec3 viewPos;
uniform vec3 lightPos;   // รับค่าตำแหน่งไฟ
uniform vec3 lightColor; // รับค่าสีไฟ
uniform vec3 fogColor;
uniform float fogDensity;

vec3 heightToTerrainColor(float h)
{
    float normH = (h + 70.0) / 200.0;
    normH = clamp(normH, 0.0, 1.0);

    vec3 water = texture(uTextureWater, vTexCoord).rgb;
    vec3 sand  = texture(uTextureSand, vTexCoord).rgb;
    vec3 grass = texture(uTextureGrass, vTexCoord).rgb;
    vec3 rock  = texture(uTextureRock, vTexCoord).rgb;
    vec3 snow  = texture(uTextureSnow, vTexCoord).rgb;

    if (normH < 0.025) return mix(water, sand, smoothstep(0.0, 0.025, normH));
    if (normH < 0.15) return mix(sand, grass, smoothstep(0.025, 0.15, normH));
    if (normH < 0.6) return mix(grass, rock, smoothstep(0.15, 0.6, normH));
    return mix(rock, snow, smoothstep(0.6, 1.0, normH));
}

void main()
{
    // 1. สีปกติจาก Texture ตามความสูง
    vec3 baseColor = heightToTerrainColor(Height);

    // 2. คำนวณ Lighting (Phong Lighting Model)
    // Ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;
    
    // Diffuse 
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // รวมแสง (ยังไม่รวม Specular)
    vec3 lightingResult = (ambient + diffuse) * baseColor;

    // 3. คำนวณหมอก (Fog)
    float distance = length(viewPos - FragPos);
    float fogFactor = exp(-pow((distance * fogDensity), 2.0));
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    
    // 4. ผสมสีที่โดนแสงแล้วเข้ากับหมอก
    vec3 finalColor = mix(fogColor, lightingResult, fogFactor);
    
    FragColor = vec4(finalColor, 1.0);
}