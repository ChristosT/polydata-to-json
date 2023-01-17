#ifndef vtkConvertToJson_h
#define vtkConvertToJson_h

#include "vtk_nlohmannjson.h"
// clang-format off
#include VTK_NLOHMANN_JSON(json.hpp) // for json
// clang-format on

/**
 * Let's define a type for JSON to make it a little easier
 * to replace the implementation in future, if needed.
 */
using vtkNJson = nlohmann::json;

class vtkPolyData;
namespace vtk
{
vtkNJson Serialize(vtkPolyData* data);
}
#endif
