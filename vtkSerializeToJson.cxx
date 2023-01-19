#include "vtkSerializeToJson.h"

#include "vtkCellArray.h"
#include "vtkCellArrayIterator.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkPolyData.h"
#include "vtkType.h"

#include "vtk_nlohmannjson.h"
// clang-format off
#include VTK_NLOHMANN_JSON(json.hpp) // for json
// clang-format on

#include <cstdint>

namespace detail
{
// Add a new section under the label "points" describing the point coordinates
void AppendPoints(vtkNJson& json, vtkPoints* points);

// Add new sections under the labels "verts", "lines", "polys", "lines" describing cell connectivity
void AppendCells(vtkNJson& json, vtkPolyData* polydata);

void AppendClassName(vtkNJson& json, const std::string& name);
std::string vtkDataTypeToString(int type);
}

namespace vtk
{
vtkNJson Serialize(vtkPolyData* data)
{
  vtkNJson json;

  // data->Print(std::cout);

  ::detail::AppendClassName(json, "vtkPolyData");

  ::detail::AppendPoints(json, data->GetPoints());

  ::detail::AppendCells(json, data);

  return json;
}
}

namespace detail
{
void AppendClassName(vtkNJson& json, const std::string& name)
{
  json["vtkClass"] = name;
}

vtkNJson CreateCellSectionStub(const std::string& name)
{
  vtkNJson json;
  AppendClassName(json, "vtkCellsArray");
  json["name"] = name;
  json["numberOfComponents"] = 1;
  json["size"] = 0;
  json["dataType"] = "UInt32Array";
  json["buffer"] = vtkNJson();
  json["values"] = vtkNJson();

  return json;
}

std::string vtkDataTypeToString(int type)
{
  switch (type)
  {
    case VTK_TYPE_FLOAT32:
      return "Float32";
    case VTK_TYPE_FLOAT64:
      return "Float64";
    case VTK_TYPE_INT64:
      std::cerr << "Int64 array is narrowed to Int32" << std::endl;
    case VTK_TYPE_INT32:
      return "Int32";
    case VTK_TYPE_UINT16:
      return "UInt16";
    case VTK_TYPE_UINT64:
      std::cerr << "UInt64 array is narrowed to Int32" << std::endl;
    case VTK_TYPE_UINT32:
      return "UInt32";
    default:
      return "Int32";
  }
}

void AppendPoints(vtkNJson& json, vtkPoints* points)
{
  vtkNJson jsonPoints;

  AppendClassName(jsonPoints, "vtkPoints");

  jsonPoints["name"] = "_points";
  jsonPoints["numberOfComponents"] = 3;
  jsonPoints["size"] = 3 * points->GetNumberOfPoints();
  jsonPoints["dataType"] = vtkDataTypeToString(points->GetDataType()) + "Array";

#if 1
  std::vector<double> rawData;
  rawData.reserve(points->GetNumberOfPoints() * 3);

  for (size_t i = 0; i < points->GetNumberOfPoints(); i++)
  {
    double p[3];
    points->GetPoint(i, p);
    rawData.insert(rawData.end(), { p[0], p[1], p[2] });
  }
  jsonPoints["buffer"] = vtkNJson();

  jsonPoints["values"] = std::move(rawData);
#else

  jsonPoints["values"] = vtkNJson::array();

  auto A = vtkNJson::array();
  A.inserinsert(static_cast<double*>(points->GetVoidPointer(0)),
    static_cast<double*>(points->GetVoidPointer(0)) + points->GetNumberOfPoints() * 3);

#endif

  double bounds[6];
  points->GetBounds(bounds);
  jsonPoints["ranges"] = vtkNJson::array();

  jsonPoints["ranges"].push_back(
    { { "min", bounds[0] }, { "max", bounds[1] }, { "component", 0 }, { "name", "X" } });

  jsonPoints["ranges"].push_back(
    { { "min", bounds[2] }, { "max", bounds[3] }, { "component", 1 }, { "name", "Y" } });

  jsonPoints["ranges"].push_back(
    { { "min", bounds[4] }, { "max", bounds[5] }, { "component", 2 }, { "name", "Z" } });

  json["points"] = jsonPoints;
}

void UpdateCellArrayEntry(vtkNJson& cells, vtkCellArray* cellArray)
{
  cells.at("dataType") = vtkDataTypeToString(cellArray->GetData()->GetDataType()) + "Array";
  cells.at("numberOfComponents") = 1;

  // copied from vtkCellArray::ExportLegacyFormat @ 65fc526a83ac829628a9462f61fa57f1801e2c7e
  std::vector<vtkIdType> rawData;
  {
    rawData.reserve(
      cellArray->GetOffsetsArray()->GetSize() + cellArray->GetConnectivityArray()->GetSize());

    auto iter = vtk::TakeSmartPointer(cellArray->NewIterator());

    vtkIdType cellSize;
    const vtkIdType* cellPoints;
    for (iter->GoToFirstCell(); !iter->IsDoneWithTraversal(); iter->GoToNextCell())
    {
      iter->GetCurrentCell(cellSize, cellPoints);
      rawData.push_back(cellSize);
      for (vtkIdType i = 0; i < cellSize; ++i)
      {
        rawData.push_back(cellPoints[i]);
      }
    }
  }

  cells.at("size") = rawData.size();
  cells.at("values") = std::move(rawData);
}

void AppendCells(vtkNJson& json, vtkPolyData* data)
{
  json["verts"] = detail::CreateCellSectionStub("_verts");
  json["lines"] = detail::CreateCellSectionStub("_lines");
  json["polys"] = detail::CreateCellSectionStub("_polys");
  json["strips"] = detail::CreateCellSectionStub("_strips");
  if (data->GetNumberOfVerts() > 0)
  {
    UpdateCellArrayEntry(json.at("verts"), data->GetVerts());
  }
  if (data->GetNumberOfLines() > 0)
  {
    UpdateCellArrayEntry(json.at("lines"), data->GetLines());
  }
  if (data->GetNumberOfPolys() > 0)
  {
    UpdateCellArrayEntry(json.at("polys"), data->GetPolys());
  }
  if (data->GetNumberOfStrips() > 0)
  {
    UpdateCellArrayEntry(json.at("strips"), data->GetStrips());
  }
}
}
