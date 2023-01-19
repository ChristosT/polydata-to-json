#include "vtkSerializeToJson.h"

#include "vtkAbstractArray.h"
#include "vtkArrayDispatch.h"
#include "vtkCell.h"
#include "vtkCellArray.h"
#include "vtkCellArrayIterator.h"
#include "vtkCellData.h"
#include "vtkDataArray.h"
#include "vtkDataSetAttributes.h"
#include "vtkDataSetAttributesFieldList.h"
#include "vtkFieldData.h"
#include "vtkIdTypeArray.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkPolyData.h"
#include "vtkType.h"

#include "vtk_nlohmannjson.h"
// clang-format off
#include VTK_NLOHMANN_JSON(json.hpp) // for json
// clang-format on

#include <chrono>
#include <cstdint>

namespace detail
{
// Add a new section under the label "points" describing the point coordinates
void AppendPoints(vtkNJson& json, vtkPoints* points);

// Add new sections under the labels "verts", "lines", "polys", "lines" describing cell connectivity
void AppendCells(vtkNJson& json, vtkPolyData* polydata);

void AppendPointData(vtkNJson& json, vtkPointData* data);
void AppendCellData(vtkNJson& json, vtkCellData* data);
void AppendFieldData(vtkNJson& json, vtkFieldData* data);
}

namespace vtk
{
vtkNJson Serialize(vtkPolyData* data)
{
  vtkNJson json;

  // data->Print(std::cout);

  json["vtkClass"] = "vtkPolyData";
  ::detail::AppendPoints(json, data->GetPoints());
  ::detail::AppendCells(json, data);
  ::detail::AppendPointData(json, data->GetPointData());
  ::detail::AppendCellData(json, data->GetCellData());
  ::detail::AppendFieldData(json, data->GetFieldData());

  return json;
}
}

namespace detail
{
void AppendClassName(vtkNJson& json, const std::string& name)
{
  json["vtkClass"] = name;
}

// Create a data array section with some default values
vtkNJson CreateDataArrayStub()
{
  vtkNJson json;
  AppendClassName(json, "vtkDataArray");
  json["name"] = "";
  json["numberOfComponents"] = 1;
  json["size"] = 0;
  json["dataType"] = "UInt32Array";
  json["buffer"] = vtkNJson();
  json["values"] = vtkNJson();
  return json;
}
// Create a data attributes section for point/cell/field data sections
vtkNJson CreateDataAttributesStub()
{
  vtkNJson json;
  AppendClassName(json, "vtkDataSetAttributes");
  json["activeGlobalIds"] = -1;
  json["activeNormals"] = -1;
  json["activePedigreeIds"] = -1;
  json["activeScalars"] = -1;
  json["activeTCoords"] = -1;
  json["activeTensors"] = -1;
  json["activeVectors"] = -1;
  json["copyFieldFlags"] = vtkNJson::array();
  json["doAllCopyOn"] = true;
  json["doAllCopyOff"] = false;

  return json;
}

// Convert VTK value types to javascript ones
// The only special thing is that int64 and uint64 are mappewd to their 32bit counterparts
std::string vtkDataTypeToString(int type)
{
  switch (type)
  {
    case VTK_TYPE_FLOAT32:
      return "Float32";
    case VTK_TYPE_FLOAT64:
      return "Float64";

    // signed types
    case VTK_TYPE_INT8:
      return "Int8";
    case VTK_TYPE_INT16:
      return "Int16";
    case VTK_TYPE_INT64:
      std::cerr << "Int64 array is narrowed to Int32" << std::endl;
    case VTK_TYPE_INT32:
      return "Int32";

    // unsigned types
    case VTK_TYPE_UINT8:
      return "UInt8";
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

// Array Dispatcher for copying values from vtkDataArray's to json arrays
struct SerializeArrayWorker
{
  vtkNJson& Json;

  SerializeArrayWorker(vtkNJson& json)
    : Json(json){};

  // Fast path
  template <typename ValueType>
  void operator()(vtkAOSDataArrayTemplate<ValueType>* array)
  {
    std::vector<ValueType> rawData;
    rawData.resize(array->GetNumberOfTuples() * array->GetNumberOfComponents());
    std::copy(array->Begin(), array->End(), rawData.begin());
    Json.at("values") = std::move(rawData);
  }

  // For SOA layouts just go element by element
  template <typename Array>
  void operator()(Array* array)
  {
    vtkDataArrayAccessor<Array> a(array);

    using ArrayType = typename vtkDataArrayAccessor<Array>::APIType;

    std::vector<ArrayType> rawData;

    const vtkIdType numItems = array->GetNumberOfTuples();
    const vtkIdType numberOfComponents = array->GetNumberOfComponents();
    rawData.reserve(numItems * numberOfComponents);
    for (vtkIdType i = 0; i < numItems; ++i)
    {
      for (vtkIdType j = 0; j < numberOfComponents; ++j)
      {
        rawData.push_back(a.Get(i, j));
      }
    }
    Json.at("values") = std::move(rawData);
  }
};

// Update a json entry created through CreateDataAttributeStub
void UpdateDataArray(vtkNJson& json, vtkDataArray* data)
{
  AppendClassName(json, "vtkDataArray");
  if (data->GetName())
  {
    json.at("name") = data->GetName();
  }
  json.at("numberOfComponents") = data->GetNumberOfComponents();
  json.at("size") = data->GetNumberOfComponents() * data->GetNumberOfTuples();
  json.at("dataType") = vtkDataTypeToString(data->GetDataType()) + "Array";

  SerializeArrayWorker worker(json);

  using Dispatcher = vtkArrayDispatch::DispatchByValueType<vtkArrayDispatch::AllTypes>;

  if (!Dispatcher::Execute(data, worker))
  {
    worker(data); // vtkDataArray & vtkSOADataArray fallback
  }
}

// Add top level  "points" section
void AppendPoints(vtkNJson& json, vtkPoints* points)
{
  vtkNJson jsonPoints = CreateDataArrayStub();
  UpdateDataArray(jsonPoints, points->GetData());

  // add point-specific fields
  AppendClassName(jsonPoints, "vtkPoints");
  jsonPoints.at("name") = "_points";

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

// update a cell connectivity entry created using CreateDataAttributeStub
void UpdateCellArrayEntry(vtkNJson& json, const std::string& name, vtkCellArray* cellArray)
{
  vtkNJson& cells = json.at(name);
  vtkNew<vtkIdTypeArray> legacy;
  cellArray->ExportLegacyFormat(legacy);

  UpdateDataArray(cells, legacy);

  // cell-specific attributes
  cells.at("name") = "_" + name;
  AppendClassName(cells, "vtkCellsArray");
}

// Add cell connectivity
void AppendCells(vtkNJson& json, vtkPolyData* data)
{
  json["verts"] = detail::CreateDataArrayStub();
  json["lines"] = detail::CreateDataArrayStub();
  json["polys"] = detail::CreateDataArrayStub();
  json["strips"] = detail::CreateDataArrayStub();

  UpdateCellArrayEntry(json, "verts", data->GetVerts());
  UpdateCellArrayEntry(json, "lines", data->GetLines());
  UpdateCellArrayEntry(json, "polys", data->GetPolys());
  UpdateCellArrayEntry(json, "strips", data->GetStrips());
}

void UpdateDataAttributeEntry(vtkNJson& json, vtkDataSetAttributes* data)
{
  std::array<int, vtkDataSetAttributes::AttributeTypes::NUM_ATTRIBUTES> attributes;
  data->GetAttributeIndices(attributes.data());
  json["activeGlobalIds"] = attributes[vtkDataSetAttributes::AttributeTypes::GLOBALIDS];
  json["activeNormals"] = attributes[vtkDataSetAttributes::AttributeTypes::NORMALS];
  json["activePedigreeIds"] = attributes[vtkDataSetAttributes::AttributeTypes::PEDIGREEIDS];
  json["activeScalars"] = attributes[vtkDataSetAttributes::AttributeTypes::SCALARS];
  json["activeTCoords"] = attributes[vtkDataSetAttributes::AttributeTypes::TCOORDS];
  json["activeTensors"] = attributes[vtkDataSetAttributes::AttributeTypes::TENSORS];
  json["activeVectors"] = attributes[vtkDataSetAttributes::AttributeTypes::VECTORS];
  bool doAllCopyOn = true;
  doAllCopyOn &= data->GetCopyAttribute(vtkDataSetAttributes::AttributeTypes::GLOBALIDS,
    vtkDataSetAttributes::AttributeCopyOperations::ALLCOPY);
  doAllCopyOn &= data->GetCopyAttribute(vtkDataSetAttributes::AttributeTypes::NORMALS,
    vtkDataSetAttributes::AttributeCopyOperations::ALLCOPY);
  doAllCopyOn &= data->GetCopyAttribute(vtkDataSetAttributes::AttributeTypes::PEDIGREEIDS,
    vtkDataSetAttributes::AttributeCopyOperations::ALLCOPY);
  doAllCopyOn &= data->GetCopyAttribute(vtkDataSetAttributes::AttributeTypes::SCALARS,
    vtkDataSetAttributes::AttributeCopyOperations::ALLCOPY);
  doAllCopyOn &= data->GetCopyAttribute(vtkDataSetAttributes::AttributeTypes::TCOORDS,
    vtkDataSetAttributes::AttributeCopyOperations::ALLCOPY);
  doAllCopyOn &= data->GetCopyAttribute(vtkDataSetAttributes::AttributeTypes::TENSORS,
    vtkDataSetAttributes::AttributeCopyOperations::ALLCOPY);
  doAllCopyOn &= data->GetCopyAttribute(vtkDataSetAttributes::AttributeTypes::VECTORS,
    vtkDataSetAttributes::AttributeCopyOperations::ALLCOPY);

  bool doAllCopyOff = ~doAllCopyOff;

  json["doAllCopyOn"] = doAllCopyOn;
  json["doAllCopyOff"] = doAllCopyOff;
}

// Get {point,cell,field}Data. It is a template since vtkFieldData is not vtkDataSetAttributes
template <typename T>
vtkNJson GetDataSetAttributes(T* data)
{
  vtkNJson json = CreateDataAttributesStub();

  json["arrays"] = vtkNJson::array();

  for (vtkIdType i = 0; i < data->GetNumberOfArrays(); i++)
  {
    if (vtkDataArray* dataArray = data->GetArray(i))
    {
      vtkNJson array = detail::CreateDataArrayStub();
      UpdateDataArray(array, data->GetArray(i));
      json.at("arrays").push_back(array);
    }
    else
    {
      std::cerr << "Abstract array at index " << i << " with name "
                << data->GetAbstractArray(i)->GetName() << " is skipped ! " << std::endl;
    }
  }

  return json;
}
void AppendPointData(vtkNJson& parent, vtkPointData* data)
{
  parent["pointData"] = GetDataSetAttributes(data);
  UpdateDataAttributeEntry(parent["pointData"], data);
}
void AppendCellData(vtkNJson& parent, vtkCellData* data)
{
  parent["cellData"] = GetDataSetAttributes(data);
  UpdateDataAttributeEntry(parent["cellData"], data);
}
void AppendFieldData(vtkNJson& parent, vtkFieldData* data)
{
  parent["fieldData"] = GetDataSetAttributes(data);
}
}
