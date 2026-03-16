#version 450

layout (location = 0) in vec3 fragColor;

layout (location = 0) out vec4 outColor;

void main() {
  // Le but est de pouvoir dessiner un rond, c'est à dire, modifier la valeur alpha à l'exterieur de mon rond et de pouvoir jouer avec le rgd à l'interieur.
  outColor = vec4(fragColor,1.0);
}
