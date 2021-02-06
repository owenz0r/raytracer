#ifdef __WINDOWS__
#include "SDL.h"
#include "glm.hpp"
#else
#include <SDL2/SDL.h>
#include "glm/glm.hpp"
#endif

#include <stdio.h>
#include <math.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

#define PI 3.141592653589793f

constexpr auto SCREEN_WIDTH = 1280;
constexpr auto SCREEN_HEIGHT = 720;

constexpr auto NUM_THREADS = 8;


class Ray {
	glm::vec3 m_origin;
	glm::vec3 m_direction;

public:
	Ray(glm::vec3 origin, glm::vec3 direction)
		: m_origin(origin), m_direction(direction) {}
	Ray()
		: m_origin(glm::vec3(0)), m_direction(glm::vec3(0)) {}

	const glm::vec3& origin() const { return m_origin; }
	const glm::vec3& direction() const { return m_direction; }
};

class Renderable {
public:
	virtual float intersect(const Ray &ray) const = 0;
	virtual const glm::vec3& position() const = 0;
	virtual const glm::vec3& colour() const = 0;
	virtual const float& diffuse() const = 0;
	virtual const float& specular() const = 0;
};

typedef std::shared_ptr<Renderable> RenderableRef;

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
		: m_radius(radius), m_position(position), m_colour(colour),
		m_diffuse(1.0f), m_specular(0.0f) {}

	float intersect( const Ray &ray ) const {
		const glm::vec3 op = m_position - ray.origin();
		constexpr float eps = 1e-4f;
		const float b = glm::dot(op, ray.direction());
		float det = b*b - glm::dot(op, op) + m_radius * m_radius;
		if (det < 0) {
			return 0;
		} else {
			det = sqrt(det);
		}
		
		float t;
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

	const float& radius() const { return m_radius; }
	const glm::vec3& position() const { return m_position; }
	const glm::vec3& colour() const { return m_colour; }
	const float& diffuse() const { return m_diffuse; }
	const float& specular() const { return m_specular; }
};

typedef std::shared_ptr<Sphere> SphereRef;

class Light : public Sphere {
	float m_intensity;

public:
	Light(float intensity, glm::vec3 position)
		: Sphere(0.05f, position, glm::vec3(255, 255, 0)), m_intensity(intensity) {};

	const float& intensity() const { return m_intensity; }
};

typedef std::shared_ptr<Light> LightRef;

void setPixel(const SDL_Surface *surface,
			  const int x,
			  const int y,
			  const unsigned short r,
			  const unsigned short g,
			  const unsigned short b,
			  const unsigned short a=255)
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

void setupScene(std::vector<Sphere> &spheres, std::vector<Light> &lights, std::vector<Renderable*> &render_objects)
{
	spheres.push_back(Sphere(1.0, glm::vec3(0, 0, -10), glm::vec3(255, 0, 0)));
	spheres.push_back(Sphere(0.5, glm::vec3(-1.5, -0.5, -8), glm::vec3(0, 255, 0)));
	spheres.push_back(Sphere(0.5, glm::vec3(1, -0.5, -6), glm::vec3(0, 0, 255)));
	// walls
	spheres.push_back(Sphere(500.0, glm::vec3(0, -501, -10), glm::vec3(200, 200, 200), 1.0f, 0.3f));
	spheres.push_back(Sphere(500.0, glm::vec3(-503, 0, -10), glm::vec3(200, 200, 200), 1.0f, 0.3f));
	spheres.push_back(Sphere(500.0, glm::vec3(0, 0, -515), glm::vec3(200, 200, 200), 1.0f, 0.3f));
	spheres.push_back(Sphere(500.0, glm::vec3(503, 0, -10), glm::vec3(200, 200, 200), 1.0f, 0.3f));

	lights.push_back(Light(1.0f, glm::vec3(-1, 1, -5)));

	for (auto &sphere : spheres)
		render_objects.push_back(&sphere);
	for (auto &light : lights)
		render_objects.push_back(&light);
}

const Ray createCameraRay(const int x,
                    const int y,
                    const float invWidth,
                    const float invHeight,
                    const float aspectratio,
                    const float angle)
{
	const float xx = (2.0f * ((x + 0.5f) * invWidth) - 1.0f) * angle * aspectratio;
	const float yy = (1.0f - 2.0f * ((y + 0.5f) * invHeight)) * angle;
	const glm::vec3 raydir(xx, yy, -1);
	return Ray(glm::vec3(0, 0, 0), glm::normalize(raydir));
}

Renderable* findClosestObject(const std::vector<Renderable*> &render_objects,
                                const Ray &ray,
                                float &closest_dist)
{
	float dist = 0.0f;
    Renderable* closest = nullptr;
	// loop render objects
	for (auto object : render_objects) {
		dist = object->intersect(ray);
		if (dist > 0 && dist < closest_dist)
		{
			closest = object;
			closest_dist = dist;
		}
	}
	return closest;
}

bool isInShadow(const std::vector<Sphere> &spheres,
				const Ray &lightray,
				const float lightdir_length)
{
	// shadow check
	bool in_shadow = false;
	for (auto sphere : spheres)
	{
		float dist = sphere.intersect(lightray);
		if (dist > 0 && dist < lightdir_length)
		{
			in_shadow = true;
			break;
		}
	}
	return in_shadow;
}

void calcIllumination(const std::vector<Light> &lights,
					  const std::vector<Sphere> &spheres,
					  const Ray &ray,
					  const Renderable* closest,
					  const glm::vec3 &contact_point,
					  const glm::vec3 &sphere_normal,
					  float &diffuse,
					  float & specular)
{
	// check lights on contact point
	for (auto light : lights)
	{
		const glm::vec3 lightdir(contact_point - light.position());
		const glm::vec3 lightdir_normalized = glm::normalize(lightdir);
		const float lightdir_length = glm::length(lightdir);
		const Ray lightray(light.position(), lightdir_normalized);

		// shadow check
		if (!isInShadow(spheres, lightray, lightdir_length))
		{
			diffuse += abs(glm::dot(lightdir_normalized, glm::normalize(contact_point - closest->position())) * light.intensity());
			specular += glm::pow(glm::dot(ray.direction(), glm::reflect(lightdir_normalized, sphere_normal)), 20.0f);
		}
	}
}

glm::vec3 calcFinalColour(Renderable* closest, float specular, float diffuse)
{
	float diffuse_scale = 1.0f;
	float specular_scale = 0.6f;
	specular = glm::clamp(specular, 0.0f, 1.0f);
	diffuse = glm::clamp(diffuse, 0.0f, 1.0f);
	glm::vec3 final_colour = (closest->colour() * closest->diffuse() * diffuse * diffuse_scale)
		+ (specular * closest->specular() * glm::vec3(255, 255, 255) * specular_scale);
	final_colour = glm::clamp(final_colour, glm::vec3(0, 0, 0), glm::vec3(255, 255, 255));
	return final_colour;
}

void raytrace(const SDL_Surface* screenSurface,
			  const std::vector<Ray> &rays,
			  const std::vector<Sphere> &spheres,
			  const std::vector<Light> &lights,
			  const std::vector<Renderable*> &render_objects)
{
	auto process = [&](const int start, const int end) {
		for (int y = start; y < end; ++y)
		{
			const unsigned stride = SCREEN_WIDTH * y;
			for (int x = 0; x < SCREEN_WIDTH; ++x)
			{
				const Ray& ray = rays[stride + x];
				float closest_dist = std::numeric_limits<float>::max();
				Renderable* closest = findClosestObject(render_objects, ray, closest_dist);

				// found closest sphere to draw
				if (closest)
				{
					float bias = 1e-4f;
					glm::vec3 contact_point(ray.origin() + (ray.direction() * closest_dist));
					glm::vec3 sphere_normal = glm::normalize(contact_point - closest->position());
					contact_point = contact_point + (bias * sphere_normal);

					float specular = 0.0f, diffuse = 0.0f;
					calcIllumination(lights, spheres, ray, closest, contact_point, sphere_normal, diffuse, specular);

					glm::vec3 final_colour = calcFinalColour(closest, specular, diffuse);
					setPixel(screenSurface, x, y, static_cast<unsigned short>(final_colour.x), static_cast<unsigned short>(final_colour.y), static_cast<unsigned short>(final_colour.z));
				}
			}
		}
	};
	
	constexpr int rows_per_thread = SCREEN_HEIGHT / NUM_THREADS;
	std::vector<std::thread> threads;
	threads.resize(NUM_THREADS);
	
	for (int i=0; i < NUM_THREADS; ++i)
		threads[i] = std::thread(process, rows_per_thread * i, std::min(rows_per_thread * (i+1), SCREEN_HEIGHT));
	
	for (int i=0; i < NUM_THREADS; ++i)
		threads[i].join();
}

int main(int argc, char* args[])
{
	std::vector<Sphere> spheres;
	std::vector<Light> lights;
	std::vector<Renderable*> render_objects;
	setupScene(spheres, lights, render_objects);

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
			
			constexpr auto num_pixels = SCREEN_WIDTH * SCREEN_HEIGHT;
			std::vector<Ray> rays;
			rays.resize(num_pixels);
			
			constexpr float invWidth = 1 / float(SCREEN_WIDTH), invHeight = 1 / float(SCREEN_HEIGHT);
			constexpr float fov = 30, aspectratio = SCREEN_WIDTH / float(SCREEN_HEIGHT);
            const float angle = tan(PI * 0.5f * fov / 180.0f);
			
			for (int y = 0; y < SCREEN_HEIGHT; ++y)
			{
				const unsigned stride = SCREEN_WIDTH * y;
				for (int x = 0; x < SCREEN_WIDTH; ++x)
					rays[stride + x] = createCameraRay(x, y, invWidth, invHeight, aspectratio, angle);
			}

			bool quit = false;
			SDL_Event e;
			while (!quit) 
			{
				auto t1 = std::chrono::high_resolution_clock::now();
				raytrace(screenSurface, rays, spheres, lights, render_objects);
				auto t2 = std::chrono::high_resolution_clock::now();
				
				auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
				std::cout << "Raytrace - " << duration << " ms" << "\n";
				
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
