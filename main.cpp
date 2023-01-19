#include <iostream>
#include <fstream>
#include "math.h"

const int width = 512;
const int height = 512;
const float EPSILON = 0.00001;

const vec3 cameraPos(0, 0, 1);
const vec3 lightPos(3, 5, 0);
const vec3 lightColor(1, 1, 1);

const vec3 spherePos0(0, 0, -3);
const float r0 = 1.0f;

float SphereSDF(vec3 pos);
float DistField(vec3 pos);
vec3 RayMarching(vec3 pos, vec3 rd, int times);
vec3 Gradient(vec3 pos);
float Shadow(vec3 p1, vec3 N, int times);
vec3 Reflect(vec3 I, vec3 N);
vec3 ReflectColor(vec3 col, vec3 p, vec3 I, vec3 N, int times, float scale);
vec3 PointShader(vec3 col, vec3 pos, vec3 N);

const float offset = 1.0f / width / 4.0f;
const float offsets[4][2] = {
	{ offset,  offset},
	{ offset, -offset},
	{-offset, -offset},
	{-offset,  offset}
};

bool SSAA = false;

int main()
{
	std::ofstream image;
	image.open("RayMarching.ppm", std::ios_base::out);
	image << "P3\n" << width << ' ' << height << '\n'<< "255\n";
	
	for (int j = height -1 ; j >= 0; --j)
	{
		for (int i = 0; i < width; ++i)
		{
			vec3 finColor(0, 0, 0);
			if (SSAA)
			{
				for (int k = 0; k < 4; ++k)
				{
					vec3 ray = vec3((float)i / width + offsets[k][0], (float)j / height + offsets[k][1], 0.0f) * 2.0f
						- vec3(1.0f, 1.0f, 0.0f)
						- cameraPos;
					ray = unit_vector(ray);
					vec3 intersectPos = RayMarching(cameraPos, ray, 100);
					vec3 normal = Gradient(intersectPos);
					vec3 color = vec3(1, 1, 1);
					if (normal.norm() == 0)
						color = vec3(0, 0, 0);
					else
					{
						normal = unit_vector(normal);
						color = PointShader(vec3(1, 1, 1), intersectPos, normal);
						color = ReflectColor(color, intersectPos, ray, normal, 100, 0.5f);
					}
					finColor += color;
				}
				finColor *= 0.25f;
			}
			else
			{
				vec3 ray = vec3((float)i / width, (float)j / height, 0.0f) * 2.0f
					- vec3(1.0f, 1.0f, 0.0f)
					- cameraPos;
				ray = unit_vector(ray);
				vec3 intersectPos = RayMarching(cameraPos, ray, 100);
				vec3 normal = Gradient(intersectPos);
				vec3 color = vec3(1, 1, 1);
				if (normal.norm() == 0)
					color = vec3(0, 0, 0);
				else
				{
					normal = unit_vector(normal);
					color = PointShader(vec3(1, 1, 1), intersectPos, normal);
					color = ReflectColor(color, intersectPos, ray, normal, 100, 0.5f);
				}
				finColor = color;
			}
			
			finColor[0] = float_clamp(finColor[0], 0, 1);
			finColor[1] = float_clamp(finColor[1], 0, 1);
			finColor[2] = float_clamp(finColor[2], 0, 1);
			finColor *= 255.999;
			int r = finColor[0];
			int g = finColor[1];
			int b = finColor[2];
			image << r << ' ' << g << ' ' << b << '\n';
		}
	}
	return 0;
}


float SphereSDF(vec3 pos)
{
	float minDist = (spherePos0 - pos).norm() - r0;
	return minDist;
}


float DistField(vec3 pos)
{
	float d1 = SphereSDF(pos);
	float d2 = pos.y() - (-1.5f);
	return min(d1, d2);	
}

vec3 RayMarching(vec3 pos, vec3 rd, int times)
{
	if (times <= 0)
		return pos;

	float minDist = DistField(pos);
	if (minDist < 0.01)
		return pos;
	else
	{
		vec3 newPos = pos + rd * minDist;
		return RayMarching(newPos, rd, times - 1);	
	}
}

//利用SDF计算某一个点的梯度，近似作为该点的normal
vec3 Gradient(vec3 pos)
{
	vec3 gradient = vec3(
		DistField(pos + vec3(EPSILON, 0, 0)) * (0.5 / EPSILON) - DistField(pos + vec3(-EPSILON, 0, 0)) * (0.5 / EPSILON),
		DistField(pos + vec3(0, EPSILON, 0)) * (0.5 / EPSILON) - DistField(pos + vec3(0, -EPSILON, 0)) * (0.5 / EPSILON),
		DistField(pos + vec3(0, 0, EPSILON)) * (0.5 / EPSILON) - DistField(pos + vec3(0, 0, -EPSILON)) * (0.5 / EPSILON));
	return gradient;
}

//计算阴影
float Shadow(vec3 p1, vec3 N, int itmes)
{
	vec3 rd = unit_vector(lightPos - p1);
	vec3 pos = RayMarching(p1 + N * 0.02, rd, 100);
	float d = (lightPos - pos).norm();
	float maxDist = (lightPos - p1).norm();
	return d < maxDist ? 0 : 1;
}


vec3 Reflect(vec3 I, vec3 N)
{
	return I - 2 * dot(I, N) * N;
}

//计算反射的颜色
vec3 ReflectColor(vec3 col, vec3 p, vec3 I, vec3 N, int times, float scale)
{
	vec3 reflectDir = Reflect(I, N);
	vec3 reflectPos = RayMarching(vec3(p + N * 0.02), reflectDir, 50);
	vec3 reflectNormal = Gradient(reflectPos);
	if (reflectNormal.norm() == 0)
		return col;
	reflectNormal = unit_vector(reflectNormal);
	
	vec3 reflectColor = PointShader(col, reflectPos, reflectNormal);
	reflectColor *= scale;
	col += reflectColor;
	if (reflectColor.norm_squared() <= 0.03)
		return col;
	else
		return ReflectColor(col, reflectPos, reflectDir, reflectNormal ,times - 1, scale * scale);
}


vec3 PointShader(vec3 baseCol, vec3 pos, vec3 normal)
{
	normal = unit_vector(normal);

	//ambient
	vec3 ambient = vec3(0.07, 0.07, 0.07);

	//diffuse
	vec3 lightDir = unit_vector(lightPos - pos);
	float NdotL = float_max(0, dot(normal, lightDir));
	vec3 diffuse = lightColor * baseCol * NdotL;

	//specular
	vec3 viewDir = unit_vector(cameraPos - pos);
	vec3 halfDir = unit_vector(lightDir + viewDir);
	float NdotH = float_max(0, dot(normal, halfDir));
	vec3 specular = lightColor * pow(NdotH, 64);

	float shadow = Shadow(pos, normal, 50);
	vec3 color = ambient + diffuse + specular;
	color *= shadow;
	return color;	
}
