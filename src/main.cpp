#include "SDL.h"
#include "glm.hpp"

#include <stdio.h>
#include <math.h>
#include <iostream>
#include <vector>

#define PI 3.141592653589793

const int SCREEN_WIDTH = 1024;
const int SCREEN_HEIGHT = 768;


class Ray {
	glm::vec3 m_origin;
	glm::vec3 m_direction;

public:
	Ray(glm::vec3 origin, glm::vec3 direction)
		: m_origin(origin), m_direction(direction) {}

	glm::vec3 origin() const { return m_origin; }
	glm::vec3 direction() const { return m_direction; }
};

class Renderable {
public:
	virtual float intersect(const Ray &ray) const = 0;
	virtual glm::vec3 position() const = 0;
	virtual glm::vec3 colour() const = 0;
	virtual float diffuse() const = 0;
	virtual float specular() const = 0;
};

class Sphere : public Renderable {
	float m_radius;
	glm::vec3 m_position;
	glm::vec3 m_colour;
	float m_diffuse;
	float m_specular;

public:
	Sphere(float radius, glm::vec3 position, glm::vec3 colour,
			float diffuse, float specular)
		: m_radius(radius), m_position(position), m_colour(colour),
			m_diffuse(diffuse), m_specular(specular) {}

	Sphere(float radius, glm::vec3 position, glm::vec3 colour)
		: m_radius(radius), m_position(position), m_colour(colour)
	{
		m_diffuse = 1.0f;
		m_specular = 0.0f;
	}

	float intersect( const Ray &ray ) const {
		glm::vec3 op = m_position - ray.origin();
		float t, eps = 1e-4;
		float b = glm::dot(op, ray.direction());
		float det = b*b - glm::dot(op, op) + m_radius * m_radius;
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

	void translate(float x, float y, float z)
	{
		glm::vec3 trans(x, y, z);
		m_position += trans;
	}

	float radius() const { return m_radius; }
	glm::vec3 position() const { return m_position; }
	glm::vec3 colour() const { return m_colour; }
	float diffuse() const { return m_diffuse; }
	float specular() const { return m_specular; }
};

class Light : public Sphere {
	float m_intensity;

public:
	Light(float intensity, glm::vec3 position)
		: Sphere(0.05f, position, glm::vec3(255, 255, 0)), m_intensity(intensity) {};

	float intensity() const { return m_intensity; }
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

void setup_scene(std::vector<Sphere> &spheres, std::vector<Light> &lights, std::vector<Renderable*> &render_objects)
{
	spheres = { Sphere(1.0, glm::vec3(0,0,-10), glm::vec3(255,0,0)),
		Sphere(0.5, glm::vec3(-1.5,-0.5,-8), glm::vec3(0,255,0)),
		Sphere(0.5, glm::vec3(1,-0.5,-6), glm::vec3(0,0,255)),
		// walls
		Sphere(500.0, glm::vec3(0,-501,-10), glm::vec3(200,200,200), 1.0f, 0.3f),
		Sphere(500.0, glm::vec3(-503, 0,-10), glm::vec3(200,200,200), 1.0f, 0.3f),
		Sphere(500.0, glm::vec3(0, 0,-515), glm::vec3(200,200,200), 1.0f, 0.3f),
		Sphere(500.0, glm::vec3(503, 0,-10), glm::vec3(200,200,200), 1.0f, 0.3f) };
	lights = { Light(1.0f, glm::vec3(-1,1,-5)) };
	//Light(0.5f, glm::vec3(0,5,-5)) };

	for (std::vector<Sphere>::iterator sphere = spheres.begin(); sphere < spheres.end(); ++sphere)
		render_objects.push_back(&(*sphere));
	for (std::vector<Light>::iterator light = lights.begin(); light < lights.end(); ++light)
		render_objects.push_back(&(*light));
}

void raytrace(SDL_Surface* screenSurface, std::vector<Sphere> &spheres, std::vector<Light> &lights, std::vector<Renderable*> &render_objects)
{
	float invWidth = 1 / float(SCREEN_WIDTH), invHeight = 1 / float(SCREEN_HEIGHT);
	float fov = 30, aspectratio = SCREEN_WIDTH / float(SCREEN_HEIGHT);
	float angle = tan(PI * 0.5 * fov / 180.0);
	float maxdist = 20;

	for (int y = 0; y < SCREEN_HEIGHT; ++y)
	{
		for (int x = 0; x < SCREEN_WIDTH; ++x)
		{
			float xx = (2 * ((x + 0.5) * invWidth) - 1) * angle * aspectratio;
			float yy = (1 - 2 * ((y + 0.5) * invHeight)) * angle;
			glm::vec3 raydir(xx, yy, -1);
			raydir = glm::normalize(raydir);
			Ray ray(glm::vec3(0, 0, 0), raydir);

			Renderable *closest = NULL;
			float diffuse_scale = 1.0f;
			float specular_scale = 0.6f;
			float dist = 0;
			float closest_dist = std::numeric_limits<float>::max();

			// loop render objects
			for (std::vector<Renderable*>::iterator obj = render_objects.begin(); obj < render_objects.end(); ++obj)
			{
				Renderable* object = (*obj);
				dist = object->intersect(ray);
				if (dist > 0 && dist < closest_dist)
				{
					closest = object;
					closest_dist = dist;
				}
			}

			// found closest sphere to draw
			if (closest)
			{
				float bias = 1e-4;
				glm::vec3 contact_point(ray.origin() + (ray.direction() * (float)closest_dist));
				glm::vec3 sphere_normal = glm::normalize(contact_point - closest->position());
				contact_point = contact_point + (bias * sphere_normal);

				float specular = 0.0f, diffuse = 0.0f;
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
						if (dist > 0 && dist < lightdir_length)
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
				glm::vec3 final_colour = (closest->colour() * closest->diffuse() * diffuse * diffuse_scale)
					+ (specular * closest->specular() * glm::vec3(255, 255, 255) * specular_scale);
				final_colour = glm::clamp(final_colour, glm::vec3(0, 0, 0), glm::vec3(255, 255, 255));
				setPixel(screenSurface, x, y, final_colour.x, final_colour.y, final_colour.z);
			}
		}
	}
}

int main(int argc, char* args[])
{
	std::vector<Sphere> spheres;
	std::vector<Light> lights;
	std::vector<Renderable*> render_objects;
	setup_scene(spheres, lights, render_objects);

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

			bool quit = false;
			SDL_Event e;
			while (!quit) 
			{
				raytrace(screenSurface, spheres, lights, render_objects);
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
								break;
							case SDLK_e:
								lights[0].translate(0.0f, 0.0f, -0.1f);
								break;
							case SDLK_d:
								lights[0].translate(0.0f, 0.0f, 0.1f);
								break;
							case SDLK_s:
								lights[0].translate(-0.1f, 0.0f, 0.0f);
								break;
							case SDLK_f:
								lights[0].translate(0.1f, 0.0f, 0.0f);
								break;
							case SDLK_q:
								lights[0].translate(0.0f, 0.1f, 0.0f);
								break;
							case SDLK_a:
								lights[0].translate(0.0f, -0.1f, 0.0f);
								break;
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