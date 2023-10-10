#version 330
#define PI 3.1415926538

const int NUM_STEPS = 32; // doesn't matter much right now
const float MIN_DIST = 0.001; // threshold for intersection
const float MAX_DIST = 1000.0; // oops we went into space

uniform vec2 u_resolution;
uniform vec3 u_camera_position;
uniform vec3 u_camera_direction;

struct Ray { float totalDist; float minDist; vec3 endPos; bool hit; };

// The distance estimator for a sphere
float sphereDE(vec3 pos, vec3 spherePos, float size) {
	return length(pos - spherePos) - size;
}

vec3	get_ray(float fov)
{
	vec3	ray;
	vec2	norm_screen;

	norm_screen.x = (2 * gl_FragCoord.x) / u_resolution.x - 1.0;
	norm_screen.y = (-2 * gl_FragCoord.y) / u_resolution.y + 1.0;
	float height = tan(fov * PI / 360);
	float width = height * (u_resolution.x / u_resolution.y);
	vec3 up = vec3(0.0f, 1.0f, 0.0f);
	vec3 right = normalize(cross(up, u_camera_direction));
	right *= width * norm_screen.x;
	up *= height * norm_screen.y;

	ray = normalize(right + up);
	return (ray);
}

Ray march(vec3 origin, vec3 direction)
{
	float rayDist = 0.0;
	float minDist = MAX_DIST;

	for (int i = 0; i < NUM_STEPS; i++) {
		vec3 current_pos = origin + rayDist * direction;

		// Use our distance estimator to find the distance
		float _distance = sphereDE(current_pos, vec3(.0), 1.0);

		minDist = _distance < minDist ? _distance : minDist;

		if (_distance < MIN_DIST) {
			// We hit an object!
			return Ray(rayDist, minDist, current_pos, true);
		}

		if (rayDist > MAX_DIST) {
			// We have gone too far
			break;
		}

		// Add the marched distance to total
		rayDist += _distance;
	}
	return Ray(MAX_DIST, minDist, origin + rayDist * direction, false);
}

out vec4 fragColor;

void main()
{
	// Normalized pixel coordinates (from 0 to 1)
	vec2 uv = gl_FragCoord.xy / u_resolution;
	uv -= 0.5;
	uv *= 2.0;
	uv = uv * u_resolution / 100.0;

	vec3	camPos = u_camera_position;
	vec3	rayDir = vec3(uv.x, uv.y, 1.0);// get_ray(60.0f);
	vec3	color = rayDir;
	
	Ray marched = march(camPos, rayDir);

	color = marched.hit ?
		vec3(marched.totalDist / pow(float(NUM_STEPS), 0.8) * 4.0) : // shading
		vec3(0.0) + vec3(pow(clamp(-1.0 * marched.minDist + 1.0, 0.0, 1.0), 4.0) / 2.0); // glow

	fragColor = vec4(color, 1.0);
}
