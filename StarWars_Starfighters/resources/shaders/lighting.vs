#version 330

// Dane wejściowe z Raylib (atrybuty wierzchołka)
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

// Macierze transformacji przekazywane automatycznie przez Raylib
uniform mat4 mvp;
uniform mat4 matModel;

// Dane wyjściowe przekazywane do Fragment Shadera
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;

void main()
{
    // Obliczanie pozycji wierzchołka w przestrzeni świata (potrzebne do oświetlenia)
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    
    // Przekazanie koordynatów tekstury i koloru wierzchołka
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    
    // Transformacja wektora normalnego do przestrzeni świata (odrzucenie translacji macierzy)
    mat3 normalMatrix = transpose(inverse(mat3(matModel)));
    fragNormal = normalize(normalMatrix * vertexNormal);

    // Ostateczna pozycja wierzchołka na ekranie
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
