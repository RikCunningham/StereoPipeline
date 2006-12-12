/************************************************************************/
/*     File: stereo.c                                                 	*/ 
/*     Date: August 1996                                                */
/*       By: Eric Zbinden					  	*/
/*      For: NASA Ames Research Center, Intelligent Mechanisms Group  	*/
/* Function: Main program for the stereo panorama pipeline	      	*/
/*    Links: 	stereo.h	stereo.c				*/
/*		filters.h       filters.c       			*/
/*		stereo_lib.h	stereolib.c				*/
/*		model_lib.h	modellib.c				*/
/*		file_lib.h	file_lib.c				*/
/*	       	stereo.default					      	*/
/*									*/
/*    Notes: Stereo.c rebuild a 3D model of the world from a stereo	*/
/*	     Panorama. St_pan->filtering->disparity_map->range_map:	*/
/*	     	-> dot cloud 3D model					*/
/*		-> 3D .nff file						*/
/*          								*/
/************************************************************************/
#ifndef NFF_H
#define NFF_H
#include <vw/Image/ImageView.h>
#include <vw/Math/Vector.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <math.h>

#include "stereo.h"

typedef struct
{
  float x;
  float y;
} POS2D;			/* xy vector or xy vertex position */ 

typedef struct
{
  float x;
  float y;
  float z;
} POS3D;			/* xyz vector or xyz vertex position */ 

typedef struct
{
  float u;
  float v;
} UV_TEX;			/* texture coordinates */        

/* polygon (triangle) structure   */

typedef struct {
  int	vtx1;	
  int	vtx2;	
  int	vtx3;
  int	gray;
} NFF_TR;			/*  triangle sommet for nff terrain model */ 

typedef struct {
  int	 pt_number;		/* number of vertices */
  int	 tr_number;		/* number of triangle */
  POS3D  *vtx;			/* array of vertices */
  UV_TEX *tex;			/* texture coordinate */
  NFF_TR *triangle;		/* array of triangle */
} NFF; 				/* nff terrain model main structure */ 

typedef struct {
  POS3D *dot;			/* pixel position in space */
  POS2D *gradients;		/* distance gradients */
  NFF	nff;			/* nff structure */
} BUFFER;			/* buffer structure */

// Function prototypes
extern void dot_to_adaptative_mesh(BUFFER *b, int width, int height, double mesh_tolerance, int max_triangles);
void dot_to_mesh(BUFFER *b, int width, int height, int h_step, int v_step);
extern void write_inventor_impl(BUFFER *b, std::string const& filename, std::string const& texture_filename);
extern void write_vrml_impl(BUFFER *b, std::string const& filename, std::string const& texture_filename);


//  Class for building and tracking a 3D mesh
class Mesh {
  BUFFER buffers;  /* structure containing the different image/data buffers */
  

public:
  Mesh() {
    buffers.dot = NULL;
  }

  ~Mesh() { this->reset(); }
  
  void reset() {
    if (buffers.dot) 
      delete [] buffers.dot;
    buffers.dot = NULL;
  }

  void build_adaptive_mesh(vw::ImageView<vw::Vector3> const& point_image, double mesh_tolerance, int max_triangles) {
    this->reset();

    buffers.dot = new POS3D[point_image.cols() * point_image.rows()];

    for (int j = 0; j < point_image.rows(); j++) {
      for (int i = 0; i < point_image.cols(); i++) {
        buffers.dot[i + (j * point_image.cols())].x = point_image(i,j)[0];
        buffers.dot[i + (j * point_image.cols())].y = point_image(i,j)[1];
        buffers.dot[i + (j * point_image.cols())].z = point_image(i,j)[2];	       
      }
    }

    dot_to_adaptative_mesh(&buffers,
                           point_image.cols(), point_image.rows(),
                           mesh_tolerance, max_triangles);

  }

  void build_simple_mesh(vw::ImageView<vw::Vector3> const& point_image, int h_step, int v_step) {
    this->reset();

    buffers.dot = new POS3D[point_image.cols() * point_image.rows()];

    for (int j = 0; j < point_image.rows(); j++) {
      for (int i = 0; i < point_image.cols(); i++) {
        buffers.dot[i + (j * point_image.cols())].x = point_image(i,j)[0];
        buffers.dot[i + (j * point_image.cols())].y = point_image(i,j)[1];
        buffers.dot[i + (j * point_image.cols())].z = point_image(i,j)[2];	       
      }
    }

    dot_to_mesh(&buffers, 
                point_image.cols(), point_image.rows(), 
                h_step, v_step );


  }

  void write_inventor(std::string const& filename, std::string const& texture_filename) {
    write_inventor_impl(&buffers, filename, texture_filename);
  }

  void write_vrml(std::string const& filename, std::string const& texture_filename) {
    write_vrml_impl(&buffers, filename, texture_filename);
  }

};

#endif /* NFF_H */

/*******/
/* END */
/*******/



















