#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D texture_diffuse1;
uniform vec3 lightPos;   // ตำแหน่งหลอดไฟ
uniform vec3 lightColor; // สีของไฟ (เช่น 1.0, 1.0, 1.0)

void main()
{    
    // 1. Ambient
    float ambientStrength = 0.2; // ความสว่างพื้นหลัง
    vec3 ambient = ambientStrength * lightColor;

    // 2. Diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0); // ห้ามค่าติดลบ
    vec3 diffuse = diff * lightColor;
            
    // รวมแสงเข้ากับ Texture
    vec3 result = (ambient + diffuse) * texture(texture_diffuse1, TexCoords).rgb;
    FragColor = vec4(result, 1.0);
}