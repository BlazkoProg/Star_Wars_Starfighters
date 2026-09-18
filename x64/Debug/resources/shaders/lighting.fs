#version 330

// Dane wejściowe z Vertex Shadera
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

// Dane wejściowe od użytkownika (Uniforms)
uniform sampler2D texture0;      // Główna tekstura modelu
uniform vec4 colDiffuse;        // Podstawowy kolor materiału z Raylib
uniform vec3 viewPos;           // Pozycja kamery (gracza) podana przez SetShaderValue

// Struktura światła zgodna z rlights.h
struct Light {
    int enabled;
    int type;
    vec3 position;
    vec3 target;
    vec4 color;
};

// Maksymalna liczba świateł obsługiwana przez rlights.h (standardowo 4)
#define MAX_LIGHTS 4
uniform Light lights[MAX_LIGHTS];
uniform vec4 ambient;           // Ogólne światło otoczenia kosmosu

// Dane wyjściowe (ostateczny kolor piksela)
out vec4 finalColor;

void main()
{
    // Pobranie koloru z tekstury i pomnożenie przez kolor materiału/wierzchołka
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec4 baseColor = texelColor * colDiffuse * fragColor;

    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(viewPos - fragPosition);
    
    vec3 lightDiffuse = vec3(0.0);
    vec3 lightSpecular = vec3(0.0);

    // Iteracja przez wszystkie zdefiniowane światła
    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (lights[i].enabled == 1)
        {
            vec3 lightDir = vec3(0.0);
            
            if (lights[i].type == 0) // LIGHT_DIRECTIONAL (np. Słońce/Gwiazda układu)
            {
                lightDir = normalize(lights[i].position - lights[i].target);
            }
            else if (lights[i].type == 1) // LIGHT_POINT (np. żarówka, wybuch)
            {
                lightDir = normalize(lights[i].position - fragPosition);
            }

            // 1. Składowa Rozproszona (Diffuse)
            float diff = max(dot(normal, lightDir), 0.0);
            lightDiffuse += lights[i].color.rgb * diff;

            // 2. Składowa Zwierciadlana (Specular - Blinn-Phong)
            // Daje efekt błyszczącego, metalowego kadłuba na X-Wingu
            vec3 halfDir = normalize(lightDir + viewDir);
            float spec = pow(max(dot(normal, halfDir), 0.0), 32.0); // 32.0 to połyskliwość (shininess)
            lightSpecular += lights[i].color.rgb * spec * 0.4; // 0.4 to intensywność blasku
        }
    }

    // Łączenie wszystkich składowych oświetlenia
    vec3 ambientResult = ambient.rgb * baseColor.rgb;
    vec3 diffuseResult = lightDiffuse * baseColor.rgb;
    vec3 specularResult = lightSpecular; // Blask odbija światło niezależnie od koloru tekstury

    // Ostateczny kolor z zachowaniem przezroczystości (Alpha) modelu
    finalColor = vec4(ambientResult + diffuseResult + specularResult, baseColor.a);
}
