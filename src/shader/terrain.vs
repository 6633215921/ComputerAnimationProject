#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out float Height;
out vec2 vTexCoord;
out vec3 FragPos;
out vec3 vNormal; // ส่ง Normal ไปคำนวณแสง

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    Height = aPos.y; 
    vTexCoord = aTexCoord;
    FragPos = vec3(model * vec4(aPos, 1.0)); 
    
    // คำนวณ Normal ให้ถูกต้องตามการหมุนของโมเดล (ถ้ามี)
    vNormal = mat3(transpose(inverse(model))) * aNormal;
    
    gl_Position = projection * view * vec4(FragPos, 1.0);
}