#include "DEM.h"
#include "GeoRef.h"
#include <vw/FileIO.h>			  
#include <vw/Stereo/SpatialTree.h>	   // for Point2D, and BBox2D
#include <vw/Stereo/SoftwareRenderer.h>
#include <vw/Image/ImageView.h>
#include <vw/Image/Manipulation.h>
#include <vw/Image/PerPixelView.h>
#include <vw/Image/ImageMath.h>

// STL includes:
#include <iostream>	 
#include <list>
#include <set>
#include <functional>

using namespace std;
using namespace vw;
using vw::stereo::SoftwareRenderer;
using vw::stereo::ePackedArray;
using vw::stereo::eColorBufferBit;
using vw::stereo::Point2D;
using vw::stereo::BBox2D;

// Enumerations and constants 
static const float kMissingDEMPixel = -FLT_MAX;


/* 
 * The bBox may be faulty if your image crosses the prime meridian or
 * crosses one of the poles.  We don't handle this case yet, but we
 * should at least detect with this has happened and generate an error
 * message.  Unfortunately, I can think of no way at the moment to
 * check for this condition in a nice, clean abstract sort of way.
 * So, beware! :)  -- mbroxton
 */
BBox3d FindBBox(ImageView<double> pointCloud) {
  BBox3d bBox;
  double x, y, z;

  bBox.xMax = bBox.yMax = bBox.zMax = -DBL_MAX;
  bBox.xMin = bBox.yMin = bBox.zMin = DBL_MAX;

  for (unsigned int i= 0; i < pointCloud.cols(); i++) {
    for (unsigned int j = 0; j < pointCloud.rows(); j++) {
      if (pointCloud(i,j,0) != MISSING_PIXEL) {
	x = pointCloud(i,j,0);
	y = pointCloud(i,j,1);
	z = pointCloud(i,j,2);
	
	if (x < bBox.xMin)
	  {
	    bBox.xMin = x;
	  }
	else if (x > bBox.xMax)
	  {
	    bBox.xMax = x;
	  }
	
	if (y < bBox.yMin)
	  {
	    bBox.yMin = y;
	  }
	else if (y > bBox.yMax)
	  {
	    bBox.yMax = y;
	  }
	
	if (z < bBox.zMin)
	  {
	    bBox.zMin = z;
	  }
	else if (z > bBox.zMax)
	  {
	    bBox.zMax = z;
	  }
      }
    }
  }

  return bBox;

}

// static void
// SetDEMDefaultValue(vil_image_view<float> dem, int numPixels)
// {

//   float defaultValue = FLT_MAX;
  
//   for (unsigned int i = 0; i < dem.ni(); i++)
//     for (unsigned int j = 0; j < dem.nj(); j++)
//     if ((dem(i,j) != kMissingDEMPixel) && 
// 	(dem(i,j) < defaultValue)) 
//       defaultValue = dem(i,j);

//   for (unsigned int i = 0; i < dem.ni(); i++)
//     for (unsigned int j = 0; j < dem.nj(); j++)
//       if (dem(i,j) == kMissingDEMPixel)
// 	dem(i,j) = defaultValue;
// }

static void SetDEMDefaultValue(ImageView<float> &dem, double defaultValue) {
  for (unsigned int i = 0; i < dem.cols(); i++)
    for (unsigned int j = 0; j < dem.rows(); j++)
      if (dem(i,j) == kMissingDEMPixel)
	dem(i,j) = defaultValue;
}

// static void
// CalcBarycentricPoint(const Point2D point, const Point2D triangle[3],
// 		     double baryPt[3])
// {
//   double u1, u2, u3, u4, v1, v2, v3, v4;

//   // vector between 3rd and 1st vertices
//   u1 = triangle[0].X() - triangle[2].X();
//   v1 = triangle[0].Y() - triangle[2].Y();

//   // vector between 3rd and 2nd vertices
//   u2 = triangle[1].X() - triangle[2].X();
//   v2 = triangle[1].Y() - triangle[2].Y();

//   // vector between 1st vertex and point
//   u3 = point.X() - triangle[0].X();
//   v3 = point.Y() - triangle[0].Y();

//   // vector between 3rd vertex and point
//   u4 = point.X() - triangle[2].X();
//   v4 = point.Y() - triangle[2].Y();
	
//   // Determine barycentric coordinates
//   // Can precompute this!
//   double denom = v1*u2 - v2*u1;

//   if (denom == 0.0)
//     return;

//   double oneOverDenom = 1.0/denom;

//   baryPt[0] = (v4*u2 - v2*u4) * oneOverDenom;
//   baryPt[1] = (v1*u3 - v3*u1) * oneOverDenom;
//   baryPt[2] = 1.0 - baryPt[0] - baryPt[1];
// }

// static bool
// PointInTriangle(double baryPt[3])
// {
//   return !((baryPt[0] < 0.0) || (baryPt[1] < 0.0) || (baryPt[2] < 0.0));
// }

// static double
// InterpolateScalar(const double baryPt[3], const double vertexValues[3])
// {
//   // Interpolate the elevations at the corners of the triangle using
//   // the barycentric coordinate of the desired point
//   double value = (baryPt[0]*vertexValues[0] +
// 		  baryPt[1]*vertexValues[1] +
// 		  baryPt[2]*vertexValues[2]);

//   return value;
// }

// Write an ENVI compatible DEM header
static void
WriteENVIHeader(const char *headerName, int width, int height, double pixelScaling, BBox3d bBox)
{
  FILE *headerFile = fopen(headerName, "w");

  fprintf(headerFile, "ENVI\n");
  fprintf(headerFile, "description = { \n");
  fprintf(headerFile, "   -- Digital Elevation Map generated by the NASA Ames Stereo Pipeline --\n");
  fprintf(headerFile, "   \n");
  fprintf(headerFile, "   The Ames Stereo Pipeline generates 3D coordinates in a planetocentric, \n");
  fprintf(headerFile, "   coordinate system as defined by the International Astronomical Union (IAU).  \n");
  fprintf(headerFile, "   The origin of this coordinate system is the planetary center of mass, and \n");
  fprintf(headerFile, "   coordinate system is right handed.\n");
  fprintf(headerFile, "   \n");
  fprintf(headerFile, "   The output of the stereo reconstruction process is are points in a cartesian \n");
  fprintf(headerFile, "   coordinate frame.  ");
  fprintf(headerFile, "   \n");
  fprintf(headerFile, "    This DEM was generated by converting the cartesion coordinates to spherical \n");
  fprintf(headerFile, "   (polar) coordinates.   Next, the radius of an areoid that defines Mars \"sea level\"\n");
  fprintf(headerFile, "   is subtracted from the radial value of each data point.  This yields a value for\n");
  fprintf(headerFile, "   elevation relative to this areoid.  The sea-level reference is given by the IAU200 Mars\n");
  fprintf(headerFile, "   areoid: a bi-axial ellipsoid with an equatorial radius of 3396 kilometers and a\n");
  fprintf(headerFile, "   polar radius of 3376.2 kilometers.\n");
  fprintf(headerFile, "   \n");
  fprintf(headerFile, "   Finally, the latitude/longitude values have been remapped using a sinusoidal (equal area)\n");
  fprintf(headerFile, "   projection.\n");
  fprintf(headerFile, "   \n");
  fprintf(headerFile, "   Bounding box:\n");
  fprintf(headerFile, "     Minimum X (left)    = %f\n", bBox.xMin);
  fprintf(headerFile, "     Minimum Y (top)     = %f\n", bBox.yMax);
  fprintf(headerFile, "     Maximum X (right)   = %f\n", bBox.xMax);
  fprintf(headerFile, "     Maximum Y (bottom)  = %f\n", bBox.yMin);
  fprintf(headerFile, "     Minimum Z           = %f\n", bBox.zMin);
  fprintf(headerFile, "     Maximum Z           = %f\n", bBox.zMax);
  fprintf(headerFile, "     Default Z           = %f\n", bBox.zMin);
  fprintf(headerFile, "}\n");
  fprintf(headerFile, "samples = %d\n", width);
  fprintf(headerFile, "lines   = %d\n", height);
  fprintf(headerFile, "bands   = 1\n");
  fprintf(headerFile, "header offset = 0\n");
  fprintf(headerFile, "map info = { Geographic Lat/Lon, 1.5, 1.5, %f, %f, %f, %f, Mars IAU 2000 Areoid, units=Degrees}\n",
	               bBox.xMin, bBox.yMax, pixelScaling, pixelScaling);
  fprintf(headerFile, "file type = ENVI Standard\n");
  fprintf(headerFile, "data type = 4\n");	   // Floating point id
  fprintf(headerFile, "interleave = bsq\n");
  fprintf(headerFile, "byte order = 0\n");	   // IEEE/Unix byte-order
  fprintf(headerFile, "\n");
  fclose(headerFile);
}


ImageView<float> GenerateDEM(ImageView<double> pointCloud, ImageView<double> texture,
                             double &dem_spacing, int &widthDEM, int &heightDEM, 
                             BBox3d *bBox, vnl_matrix<double> &geoTransform) {
  
  unsigned int width = pointCloud.cols();
  unsigned int height = pointCloud.rows();
  POS3D *coords;
  double* textures;

  try {
    coords = new POS3D[width * height];
    textures = new double[width * height];
  } catch (std::bad_alloc &e) {
    throw vw::NullPtrErr() << "FATAL ERROR in GenerateDEM: cannot allocate DEM " 
			   << "buffers. Aborting.\n";
  }

  /* The software renderer is expecting an array of POS3D's */
  for (unsigned int i = 0; i < pointCloud.cols(); i++) {
    for (unsigned int j = 0; j < pointCloud.rows(); j++) {
      coords[j * width + i].x = pointCloud(i,j,0);
      coords[j * width + i].y = pointCloud(i,j,1);
      coords[j * width + i].z = pointCloud(i,j,2);
      textures[j * width + i] = texture(i,j,0);      
    }
  }

  *bBox = FindBBox(pointCloud);
  cout << "\tDEM BBox found = "
       << "Min[" << bBox->xMin << "," << bBox->yMin << "] "
       << "Max[" << bBox->xMax << "," << bBox->yMax << "]"
       << endl;
  printf("\tZ Range = [zmin, zmax] = [%lf, %lf]\n", bBox->zMin, bBox->zMax);

  // If no DEM spacing is explicitly supplied, generate a DEM with the
  // same pixel dimensions as the input image.  Note, however, that
  // this could lead to a loss in DEM resolution if the DEM is rotated
  // from the orientation of the original image.
  if (dem_spacing == 0.0) {
    dem_spacing = abs(bBox->xMax - bBox->xMin) / width;
    cout << "\tAutomatically setting spacing to " << dem_spacing << " units/pixel.\n";
  }

  widthDEM  = (int) (abs(bBox->xMax - bBox->xMin) / dem_spacing) + 1;
  heightDEM = (int) (abs(bBox->yMax - bBox->yMin) / dem_spacing) + 1;
  cout << "\tDEM Dimensions = [" << widthDEM << ", " << heightDEM << "]\n";

  bBox->xMax = bBox->xMin + (widthDEM * dem_spacing);
  bBox->yMax = bBox->yMin + (heightDEM * dem_spacing);

  ImageView<float> demImage(widthDEM, heightDEM, 1);
  float *dem = &(demImage(0,0));

  const int numColorComponents = 1;	   // We only need gray scale
  const int numVertexComponents = 2;	   // DEMs are 2D
  // Setup a software renderer of the same size as the DEM, and with
  // limits set to the bounding box
  SoftwareRenderer renderer = SoftwareRenderer(widthDEM, heightDEM, dem);
  renderer.Ortho2D(bBox->xMin, bBox->xMax, bBox->yMin, bBox->yMax);
  renderer.ClearColor(kMissingDEMPixel, kMissingDEMPixel, kMissingDEMPixel, 1.0);
  renderer.Clear(eColorBufferBit);
  // Render the triangles...
  // Note 1: the triangles are in lat/lon space, not row/col space
  // Note 2: We assume the topology is the same in lat/lon space as in
  // row/col space... this is not the case at the poles, and at the
  // prime meridian!

  cout << "\tRe-rendering DEM on planetary surface..." << flush;
  int numTriangles = 0;
  for (unsigned int row = 0; row < (height-1); row++)
  {
    for (unsigned int col = 0; col < (width-1); col++)
    {
      float vertices[12], intensities[6];
      int triangleCount;

      // Create the two triangles covering this "pixel"
      CreateTriangles(row, col, width, coords, textures,
		      triangleCount, vertices, intensities);

      renderer.SetVertexPointer(numVertexComponents, ePackedArray, vertices);

      // Draw elevations into DEM buffer
      renderer.SetColorPointer(numColorComponents, ePackedArray, intensities);
      for (int i = 0; i < triangleCount; i++)
	renderer.DrawPolygon(i * 3, 3);

      numTriangles += triangleCount;
    }
  }

  cout << "done." << endl;
  cout << "\tNumber of planet surface triangles = " << numTriangles << endl;
  
  /* Set up the georeferencing transform */
  geoTransform.set_size(3,3);
  geoTransform.set_identity();
  geoTransform(0,0) = dem_spacing;
  geoTransform(1,1) = -1 * dem_spacing;
  geoTransform(0,2) = bBox->xMin;
  geoTransform(1,2) = bBox->yMax; 


  /* Clean up */
  delete [] coords;
  delete [] textures;

  // The software renderer returns an image which will render upside
  // down in most image formats
  return vw::flip_vertical(demImage);
}

/* 
 * NOTE:
 *
 * According to: mars.jpl.nasa.gov/mgs/status/nav/nav.html
 * Mars Equatorial Radius : 3394.5 x 3399.2 km
 * Mars Polar Radius 	  : 3376.1 km
 * 
 * However, according to: www.isprs.org/istanbul2004/comm4/papers/455.pdf
 * the IAU 2000 standard bi-axial ellipsoid for Mars is as follows:
 *
 * Mars Equatorial Radius : 3396.19 km
 * Mars Polar Radius      : 3376.20 km
 */
void WriteDEM(ImageView<double> pointCloud,
              ImageView<double> texture,
              std::string filename_prefix,
              double dem_spacing=0)
{
  VW_ASSERT(pointCloud.cols() == texture.cols() && pointCloud.rows() == texture.rows(),
            vw::ArgumentErr() << "Point cloud and texture image must have the "
            << "same dimensions.");
  VW_ASSERT(texture.planes() == 1,
            vw::ArgumentErr() << "Currently, only one channel textures are supported.");
  
  int widthDEM, heightDEM;
  double spacing = dem_spacing;
  BBox3d bBox;  

  
  /* 
   * Call out to the software renderer to reproject and 
   * re-rasterize the DEM 
   */
  printf("Generating DEM file.\n");  
  vnl_matrix<double> geoTransform;
  ImageView<float> demImage = GenerateDEM(pointCloud, texture,
                                          spacing, widthDEM, heightDEM, 
                                          &bBox, geoTransform);
  SetDEMDefaultValue(demImage, bBox.zMin);

  /* 
   * Uncomment to convert DEM to 16-bit integer values
   */
  //  ImageView<vw::int16> intDemImage;
  //  vil_convert_cast(demImage, intDemImage);
  
  /* Write ENVI DEM
   * 
   * The GDAL ENVI file handler does not embed the georeferencing
   * information, so we must write our own ENVI header file here.
   */
  double lon_0 = bBox.xMin + ((bBox.xMax - bBox.xMin) / 2.0);
  write_georef_file(demImage, filename_prefix + ".dem", 
                    "ENVI", geoTransform, lon_0);
  std::string enviHeaderName = filename_prefix + ".hdr";
  cout << "\tWriting ENVI Header: " << enviHeaderName << ".\n";
  WriteENVIHeader(enviHeaderName.c_str(), 
                  widthDEM, heightDEM, 
                  spacing, bBox);
  
  /* Write GEOTIFF files */
  write_georef_file(demImage, filename_prefix + ".tif", 
                    "GTiff", geoTransform, lon_0);
}

/* 
 * NOTE:
 *
 * According to: mars.jpl.nasa.gov/mgs/status/nav/nav.html
 * Mars Equatorial Radius : 3394.5 x 3399.2 km
 * Mars Polar Radius 	  : 3376.1 km
 * 
 * However, according to: www.isprs.org/istanbul2004/comm4/papers/455.pdf
 * the IAU 2000 standard bi-axial ellipsoid for Mars is as follows:
 *
 * Mars Equatorial Radius : 3396.19 km
 * Mars Polar Radius      : 3376.20 km
 */
void WriteDRG(ImageView<double> pointCloud,
	      ImageView<PixelGray<double> > texture,
	      std::string filename_prefix,
	      double dem_spacing = 0)
{
  VW_ASSERT(pointCloud.cols() == texture.cols() && pointCloud.rows() == texture.rows(),
	    vw::ArgumentErr() << "Point cloud and texture image must have the "
	                      << "same dimensions.");
  VW_ASSERT(texture.planes() == 1 && texture.channels() == 1,
	    vw::ArgumentErr() << "Currently, only one channel textures are supported.");

  int widthDEM, heightDEM;
  BBox3d bBox;  
  double spacing = dem_spacing;
  
  /* 
   * Call out to the software renderer to reproject and 
   * re-rasterize the DEM 
   */
  printf("Generating DEM file.\n");  
  vnl_matrix<double> geoTransform;
  ImageView<double> temporary(texture.cols(), texture.rows(), 1);
  temporary = vw::channels_to_planes(texture);
  ImageView<float> demImage = GenerateDEM(pointCloud, temporary,
					  spacing, widthDEM, heightDEM, 
					  &bBox, geoTransform);

  printf("\tFinding DEM min/max, number of DEM pixels = %d.\n", widthDEM * heightDEM);
  printf("\tDEM [min, max] = [%lf, %lf]\n", bBox.zMin, bBox.zMax);
  SetDEMDefaultValue(demImage, 0.0);

  // Uncomment to convert DEM to integer values
  ImageView<uint8> intDemImage = vw::channel_cast<uint8>(demImage * 255);
  
  printf("\tWriting DRG.\n");
  double lon_0 = bBox.xMin + ((bBox.xMax - bBox.xMin) / 2.0);
  write_georef_file(intDemImage, filename_prefix + ".tif", 
                    "GTiff", geoTransform, lon_0);
  write_georef_file(intDemImage, filename_prefix + ".dem", 
                    "ENVI", geoTransform, lon_0);
}