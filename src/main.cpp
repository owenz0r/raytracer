#include "SDL.h"
#include "glm.hpp"

#include <stdio.h>
#include <math.h>
#include <iostream>
#include <vector>

#define PI 3.141592653589793

const int SCREEN_WIDTH = 1024;
const int SCREEN_HEIGHT = 768;

class Light {
	glm::vec3 m_position;
	float m_intensity;

public:
	Light(float intensity, glm::vec3 position )
		: m_position(position), m_intensity(intensity) {};

	glm::vec3 position() const { return m_position; }
	float intensity() const { return m_intensity; }
};

class Ray {
	glm::vec3 m_origin;
	glm::vec3 m_direction;

public:
	Ray(glm::vec3 origin, glm::vec3 direction)
		: m_origin(origin), m_direction(direction) {}

	glm::vec3 origin() const { return m_origin; }
	glm::vec3 direction() const { return m_direction; }
};

class Sphere {
	double m_radius;
	glm::vec3 m_position;
	glm::vec3 m_colour;

public:
	Sphere(double radius, glm::vec3 position, glm::vec3 colour)
		: m_radius(radius), m_position(position), m_colour(colour) {}

	double intersect( const Ray &ray ) const {
		glm::vec3 op = m_position - ray.origin();
		double t, eps = 1e-4;
		double b = glm::dot(op, ray.direction());
		double det = b*b - glm::dot(op, op) + m_radius * m_radius;
		if (det < 0) {
			return 0;
		} else {
			det = sqrt(det);
		}

		if ((t = b - det) > eps) {
			return t;
		} else {
			if ((t = b + det) > eps) {
				return t;
			}
			else 
			{
				return 0;
			}
		}
	}

	double radius() const { return m_radius; }
	glm::vec3 position() const { return m_position; }
	glm::vec3 colour() const { return m_colour; }
};

void setPixel(SDL_Surface *surface, int x, int y, unsigned short r, unsigned short g, unsigned short b, unsigned short a=255)
{
	Uint32 pixel, alpha, red, green, blue;
	pixel = alpha = red = green = blue = 0;

	alpha = a << 24;
	red = r << 16;
	green = g << 8;
	blue = b;

	pixel = pixel | alpha;
	pixel = pixel | red;
	pixel = pixel | green;
	pixel = pixel | blue;

	Uint32 *pixels = (Uint32*)surface->pixels;
	pixels[(y * surface->w) + x] = pixel;
}

int main(int argc, char* args[])
{
	std::vector<Sphere> spheres = { Sphere(1.0, glm::vec3(0,0,-10), glm::vec3(255,0,0) ),
									Sphere(50.0, glm::vec3(0,-51,-10 ), glm::vec3(255,255,255)) };
	std::vector<Light> lights = { Light(1.0f, glm::vec3(0,5,-5)) };
									//Light(0.5f, glm::vec3(0,5,-5)) };

	SDL_Window* window = NULL;
	SDL_Surface* screenSurface = NULL;
	SDL_Renderer* renderer = NULL;

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("SDL couldnt start");
	}
	else
	{
		window = SDL_CreateWindow( "Tracer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
		if (window == NULL)
		{
			printf("Couldnt create window");
		}
		else
		{
			screenSurface = SDL_GetWindowSurface(window);
			renderer = SDL_GetRenderer(window);

			Ray ray(glm::vec3(5, 1, 0), glm::vec3(-1, 0, 0));

			float invWidth = 1 / float(SCREEN_WIDTH), invHeight = 1 / float(SCREEN_HEIGHT);
			float fov = 30, aspectratio = SCREEN_WIDTH / float(SCREEN_HEIGHT);
			float angle = tan(PI * 0.5 * fov / 180.0);
			float maxdist = 20;

			bool quit = false;
			SDL_Event e;
			while (!quit) 
			{

				// raytrace
				for (int y = 0; y < SCREEN_HEIGHT; ++y)
				{
					for (int x = 0; x < SCREEN_WIDTH; ++x)
					{
						float xx = (2 * ((x + 0.5) * invWidth) - 1) * angle * aspectratio;
						float yy = (1 - 2 * ((y + 0.5) * invHeight)) * angle;
						glm::vec3 raydir(xx, yy, -1);
						raydir = glm::normalize(raydir);
						Ray ray(glm::vec3(0, 0, 0), raydir);
						
						double dist = 0;
						double dot = 0;
						Sphere *closest = NULL;
						double closest_dist = 9999;
						// loop spheres
						for (std::vector<Sphere>::iterator it = spheres.begin(); it < spheres.end(); ++it)
						{
							dist = it->intersect(ray);
							if (dist > 0 && dist < closest_dist )
							{
								closest = &(*it);
								closest_dist = dist;	
							}
						}

						// found closest sphere to draw
						if (closest)
						{
							dot = 0;
							float bias = 1e-4;
							glm::vec3 contact_point(ray.origin() + (ray.direction() * (float)closest_dist));
							glm::vec3 sphere_normal = glm::normalize(contact_point - closest->position());
							contact_point = contact_point + (bias * sphere_normal);

							
							float illumination = 0.0f, specular = 0.0f, diffuse = 0.0f;
							// check lights on contact point
							for (std::vector<Light>::iterator light = lights.begin(); light < lights.end(); ++light)
							{		
								glm::vec3 lightdir(contact_point - light->position());
								glm::vec3 lightdir_normalized = glm::normalize(lightdir);
								float lightdir_length = glm::length(lightdir);
								Ray lightray(light->position(), lightdir_normalized);

								// shadow check
								bool in_shadow = false;
								for (std::vector<Sphere>::iterator sphere = spheres.begin(); sphere < spheres.end(); ++sphere)
								{
									float dist = sphere->intersect(lightray);
									if ( dist > 0 && dist < lightdir_length )
									{
										in_shadow = true;
										break;
									}
								}
								if (!in_shadow)
								{
									diffuse += abs(glm::dot(lightdir_normalized, glm::normalize(contact_point - closest->position())) * light->intensity());
									specular += glm::pow(glm::dot(ray.direction(), glm::reflect(lightdir_normalized, sphere_normal)), 20);
								}
							}

							specular = glm::clamp(specular, 0.0f, 1.0f);
							diffuse = glm::clamp(diffuse, 0.0f, 1.0f);
							glm::vec3 final_colour = (closest->colour() * diffuse) + (specular * glm::vec3(255,255,255));
							final_colour = glm::clamp(final_colour, glm::vec3(0,0,0), glm::vec3(255,255,255));
							setPixel(screenSurface, x, y, final_colour.x, final_colour.y, final_colour.z);
						}
					}
				}
				SDL_UpdateWindowSurface(window);

				// poll events
				while (SDL_PollEvent(&e) != 0)
				{
					if (e.type == SDL_QUIT)
					{
						quit = true;
					}
					else if (e.type == SDL_KEYDOWN)
					{
						switch (e.key.keysym.sym)
						{
							case SDLK_ESCAPE:
								quit = true;
						}
					}
				}
			}
		}
	}
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}