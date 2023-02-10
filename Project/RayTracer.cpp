/*==================================================================================
* COSC 363  Computer Graphics (2022)
* Department of Computer Science and Software Engineering, University of Canterbury.
*
* A basic ray tracer
* See Lab07.pdf  for details.
*===================================================================================
*/
#include <iostream>
#include <cmath>
#include <vector>
#include <glm/glm.hpp>
#include "Sphere.h"
#include "SceneObject.h"
#include "Ray.h"
#include "Plane.h"
#include "TextureBMP.h"
#include <GL/freeglut.h>
using namespace std;

const float EDIST = 40.0;
const int NUMDIV = 500;
const int MAX_STEPS = 5;
const float XMIN = -10.0;
const float XMAX = 10.0;
const float YMIN = -10.0;
const float YMAX = 10.0;
const bool ANTIALIASING = true;
const bool FOG = false;

vector<SceneObject*> sceneObjects;
TextureBMP texture;
TextureBMP texture2;


//-----------------------------------
//    Function that constructs a pyramid
//    made of a set of planes.
//-----------------------------------
void pyramid(float x, float y, float z, float size)
{
	glm::vec3 a(x + size, y, z);
	glm::vec3 b(x, y, z + size);
	glm::vec3 c(x - size, y, z);
	glm::vec3 d(x, y, z - size);
	glm::vec3 e(x, y + size, z);

	Plane *planeA = new Plane (a, b, e);
	Plane *planeB = new Plane (b, c, e);
	Plane *planeC = new Plane (c, d, e);
	Plane *planeD = new Plane (a, d, e);
	Plane *base = new Plane (a, b, c, d);

	sceneObjects.push_back(planeA);
	sceneObjects.push_back(planeB);
	sceneObjects.push_back(planeC);
	sceneObjects.push_back(planeD);
	sceneObjects.push_back(base);
}


//---The most important function in a ray tracer! ---------------------------------- 
//   Computes the colour value obtained by tracing a ray and finding its 
//     closest point of intersection with objects in the scene.
//----------------------------------------------------------------------------------
glm::vec3 trace(Ray ray, int step)
{
	glm::vec3 backgroundCol(0);						//Background colour = (0,0,0)
	if (FOG) backgroundCol += 1;
	glm::vec3 lightPos(40, 30, -40);					//Light's position
	glm::vec3 color(0);
	SceneObject* obj;

    ray.closestPt(sceneObjects);					//Compare the ray with all objects in the scene
    if(ray.index == -1) return backgroundCol;		//no intersection
	obj = sceneObjects[ray.index];					//object on which the closest point of intersection is found

	if (ray.index == 0) //Floor
	{
		//Stripe pattern
		int tileSize = 10; //Side size (e.g. tile is 5x5)
		int iz = (ray.hit.z) / tileSize;
		int ix = (ray.hit.x) / tileSize + tileSize;
		int k = (iz + ix) % 2;
		if (k == 0) color = glm::vec3(1, 1, 1);
		else color = glm::vec3(0, 0, 0);
		obj->setColor(color);

		//Add code for texture mapping here
		float z1 = -60.;
		float z2 = -90.;
		float x1 = -15.;
		float x2 = 5.;
		float texcoords = (ray.hit.x - x1) / (x2 - x1);
		float texcoordt = (ray.hit.z - z1) / (z2 - z1);
		if(texcoords > 0 && texcoords < 1 &&
		texcoordt > 0 && texcoordt < 1)
		{
			color=texture.getColorAt(texcoords, texcoordt);
			obj->setColor(color);
		}
	}

	if (ray.index == 1) //Earth
	{
		glm::vec3 center(-10., 10., -180.);
		glm::vec3 diff = glm::normalize(ray.hit - center);
		float y = 0.5 + asin(diff.y) / M_PI;
		float x = (0.5 - atan2(diff.z, diff.x) + M_PI) / (2 * M_PI);
		color = texture2.getColorAt(x, y);
		obj->setColor(color);
	}


	// color = obj->getColor();
	color = obj->lighting(lightPos, -ray.dir, ray.hit);
	glm::vec3 lightVec = lightPos - ray.hit;
	Ray shadowRay(ray.hit, lightVec);
	shadowRay.closestPt(sceneObjects); //Find closest point of intersection on the shadow ray
	float lightDist = glm::length(lightVec);


	if ((shadowRay.index > -1) && (shadowRay.dist < lightDist)){
		if (sceneObjects[shadowRay.index]->isTransparent()) { //Define transparents objects' shadows "" && ! obj->isTransparent()
			// float transpCoef = sceneObjects[shadowRay.index]->getTransparencyCoeff();
			// color = 0.5f * transpCoef * obj->getColor();
			color = 0.6f * obj->getColor();
		} else if (sceneObjects[shadowRay.index]->isRefractive()) {
			// float refrCoef = sceneObjects[shadowRay.index]->getRefractionCoeff();
			// color = refrCoef * obj->getColor();
			color = 0.6f * obj->getColor();
		} else color = 0.2f * obj->getColor(); //0.2 = ambient scale factor
	}
	if (obj->isReflective() && step < MAX_STEPS) {
		float rho = obj->getReflectionCoeff();
		glm::vec3 normalVec = obj->normal(ray.hit);
		glm::vec3 reflectedDir = glm::reflect(ray.dir, normalVec); //r
		Ray reflectedRay(ray.hit, reflectedDir);
		glm::vec3 reflectedColor = trace(reflectedRay, step + 1);
		color = color + (rho * reflectedColor);
	}
	if (obj->isTransparent() && step < MAX_STEPS) {
		float transpCoef = obj->getTransparencyCoeff();
		Ray transparentRay(ray.hit, ray.dir);
		glm::vec3 transparentColour = trace(transparentRay, step + 1);
		color = color + (transpCoef * transparentColour);
	}
	if (obj->isRefractive() && step < MAX_STEPS) {
		float refrIndex = obj->getRefractiveIndex();
		// float refrCoef = obj->getRefractionCoeff();

		glm::vec3 n = obj->normal(ray.hit);
		glm::vec3 g = glm::refract(ray.dir, n, 1.0f/refrIndex);
		Ray refractedRay1(ray.hit, g);
		refractedRay1.closestPt(sceneObjects);

		glm::vec3 m = obj->normal(refractedRay1.hit);
		glm::vec3 h = glm::refract(g, -m, refrIndex);	//refrIndex/1 = refrindex
		Ray refractedRay2(refractedRay1.hit, h);
		refractedRay2.closestPt(sceneObjects);

		glm::vec3 refractedColor = trace(refractedRay2, step + 1);
		color = color + refractedColor;
	}
	if (FOG) {
		float z1 = -60.0;
		float z2 = -200.0;
		float t = 0.;
		if (ray.hit.z <= z1 && ray.hit.z >= z2) t = (ray.hit.z - z1)/(z2 - z1);
		color = (1 - t) * color + t * glm::vec3(1., 1., 1.);
	}

	if (ray.index > 4 && ray.index < 10) //Draws the pattern for the pyramid
	{
		int patternType = 2;
		int ix = (ray.hit.x) / patternType - 3 * patternType;
		int iy = (ray.hit.y) / patternType;
		int iz = (ray.hit.z) / patternType;
		int k = (ix + iy + iz) % 2;
		if (k == 0) color = glm::vec3(1., 1., 1.);
		else color = glm::vec3(0., 0., 0.);
		obj->setColor(color);
	}

	return color;
}

//---The main display module -----------------------------------------------------------
// In a ray tracing application, it just displays the ray traced image by drawing
// each cell as a quad.
//---------------------------------------------------------------------------------------
void display()
{
	float xp, yp;  //grid point
	float cellX = (XMAX - XMIN) / NUMDIV;  //cell width
	float cellY = (YMAX - YMIN) / NUMDIV;  //cell height
	glm::vec3 eye(0., 0., 10.);

	glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

	glBegin(GL_QUADS);  //Each cell is a tiny quad.

	if (ANTIALIASING) {
		for(int i = 0; i < NUMDIV; i++)	//Scan every cell of the image plane
		{
			xp = XMIN + i * cellX;
			for(int j = 0; j < NUMDIV; j++)
			{
				yp = YMIN + j * cellY;

				// glm::vec3 dir(xp + 0.5 * cellX, yp + 0.5 * cellY, -EDIST);	//direction of the primary ray
				glm::vec3 dir1(xp + 0.25 * cellX, yp + 0.25 * cellY, -EDIST);	
				glm::vec3 dir2(xp + 0.25 * cellX, yp + 0.75 * cellY, -EDIST);	
				glm::vec3 dir3(xp + 0.75 * cellX, yp + 0.25 * cellY, -EDIST);	
				glm::vec3 dir4(xp + 0.75 * cellX, yp + 0.75 * cellY, -EDIST);	

				Ray ray1 = Ray(eye, dir1);
				Ray ray2 = Ray(eye, dir2);
				Ray ray3 = Ray(eye, dir3);
				Ray ray4 = Ray(eye, dir4);

				// glm::vec3 col = trace (ray, 1); //Trace the primary ray and get the colour value
				glm::vec3 col1 = trace (ray1, 1);
				glm::vec3 col2 = trace (ray2, 1);
				glm::vec3 col3 = trace (ray3, 1);
				glm::vec3 col4 = trace (ray4, 1);

				glm::vec3 col = (col1 + col2 + col3 + col4) * 0.25f;

				glColor3f(col.r, col.g, col.b);
				glVertex2f(xp, yp);				//Draw each cell with its color value
				glVertex2f(xp + cellX, yp);
				glVertex2f(xp + cellX, yp + cellY);
				glVertex2f(xp, yp + cellY);
			}
		}
	} else {
		for(int i = 0; i < NUMDIV; i++)	//Scan every cell of the image plane
		{
			xp = XMIN + i * cellX;
			for(int j = 0; j < NUMDIV; j++)
			{
				yp = YMIN + j * cellY;

				glm::vec3 dir(xp + 0.5 * cellX, yp + 0.5 * cellY, -EDIST);	//direction of the primary ray

				Ray ray = Ray(eye, dir);

				glm::vec3 col = trace (ray, 1); //Trace the primary ray and get the colour value
				glColor3f(col.r, col.g, col.b);
				glVertex2f(xp, yp);				//Draw each cell with its color value
				glVertex2f(xp + cellX, yp);
				glVertex2f(xp + cellX, yp + cellY);
				glVertex2f(xp, yp + cellY);
			}
		}
	}

	

    glEnd();
    glFlush();
}



//---This function initializes the scene ------------------------------------------- 
//   Specifically, it creates scene objects (spheres, planes, cones, cylinders etc)
//     and add them to the list of scene objects.
//   It also initializes the OpenGL 2D orthographc projection matrix for drawing the
//     the ray traced image.
//----------------------------------------------------------------------------------
void initialize()
{
    glMatrixMode(GL_PROJECTION);
    gluOrtho2D(XMIN, XMAX, YMIN, YMAX);

    glClearColor(0, 0, 0, 1);

	//Object of index 0
	Plane *plane = new Plane (glm::vec3(-40., -15, -40),	//Point A
							glm::vec3(40., -15, -40),	//Point B
							glm::vec3(40., -15, -200),	//Point C
							glm::vec3(-40., -15, -200));	//Point D
	plane->setColor(glm::vec3(0.8, 0.8, 0));
	sceneObjects.push_back(plane);

	//Object of index 1 (World)
	Sphere *sphere1 = new Sphere(glm::vec3(-10.0, 10, -180.0), 15.0);
	sphere1->setSpecularity(true);
	sphere1->setReflectivity(false);
	// sphere1->setColor(glm::vec3(0, 0, 1));   //Set colour to blue
	sceneObjects.push_back(sphere1);		 //Add sphere to scene objects

	//Object of index 2 (Transparent sphere on top of pyramid)
	Sphere *sphere2 = new Sphere(glm::vec3(-10.0, -1.0, -80.0), 4.0);
	sphere2->setColor(glm::vec3(0.1, 0.0, 0.0));
	sphere2->setReflectivity(true, 0.1);
	sphere2->setTransparency(true, 0.7);
	sceneObjects.push_back(sphere2);		 //Add sphere to scene objects

	//Object of index 3 (Refracted sphere)
	Sphere *sphere3 = new Sphere(glm::vec3(5.0, -10.0, -60.0), 5.0);
	sphere3->setColor(glm::vec3(0.1, 0.1, 0.1));
	sphere3->setReflectivity(true, 0.1);
	sphere3->setRefractivity(true, 0.7518, 1.33); // Water sphere
	sceneObjects.push_back(sphere3);		 //Add sphere to scene objects
	
	//Object of index 4 (Comparison sphere)
	Sphere *sphere4 = new Sphere(glm::vec3(14.0, -12, -70.0), 3.0);
	sphere4->setColor(glm::vec3(0, 1, 1));   //Set colour to cyan
	sphere4->setReflectivity(true, 0.5);
	sceneObjects.push_back(sphere4);		 //Add sphere to scene objects

	//Object of index 5 to 9
	pyramid(-10., -15, -80, 10);

	texture = TextureBMP("Butterfly.bmp");
	texture2 = TextureBMP("Earth.bmp");
}


int main(int argc, char *argv[]) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB );
    glutInitWindowSize(500, 500);
    glutInitWindowPosition(20, 20);
    glutCreateWindow("Raytracing");

    glutDisplayFunc(display);
    initialize();

    glutMainLoop();
    return 0;
}
