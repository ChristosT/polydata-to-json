/**
 * Serialize a vtk polydata file into a vtk.js comparison json representation
 * See  https://kitware.github.io/vtk-js/docs/structures_PolyData.html
 * for format reference
 */

#include "vtkPolyData.h"
#include "vtkPolyDataReader.h"
#include "vtkSmartPointer.h"
#include "vtkXMLPolyDataReader.h"
#include "vtksys/SystemTools.hxx"

#include "vtk_cli11.h"

#include "vtkSerializeToJson.h"
#include <utility>
#include <vector>
#include <vtkcli11/CLI/Validators.hpp>

vtkNJson GetMetaData(const std::string& filename)
{
  vtkNJson data;

  data["name"] = vtksys::SystemTools::GetFilenameName(filename);
  data["size"] = vtksys::SystemTools::FileLength(filename);
  return data;
}

void Dump(const vtkNJson& json, const std::string& format)
{
  std::vector<uint8_t> binary;
  if (format == "ascii")
  {
    std::cout << json.dump();
  }
  else if (format == "bson")
  {
    binary = vtkNJson::to_bson(json);
  }
  else if (format == "cbor")
  {
    binary = vtkNJson::to_cbor(json);
  }
  else if (format == "ubjson")
  {
    binary = vtkNJson::to_ubjson(json);
  }
  else if (format == "messagePack")
  {
    binary = vtkNJson::to_msgpack(json);
  }
  if (!binary.empty())
  {
    std::cout.write(reinterpret_cast<const char*>(binary.data()), binary.size());
  }
}

int main(int argc, char** argv)
{
  CLI::App app{ "Polydata to vtk.js compatible json representation" };

  struct Args
  {
    std::string filename = "default";
    std::string format = "ascii";
    std::string output = "";
  };
  Args args;
  app.add_option("--file,-f", args.filename, "Polydata file in legacy vtk or XML format")
    ->required()
    ->check(CLI::ExistingFile);
  app.add_option("--format,-t", args.format, "Serialization format.")
    ->default_str("ascii")
    ->check(CLI::IsMember({ "ascii", "bson", "cbor", "messagePack", "ubjson" }));
  app.add_option(
    "--output,-o", args.output, "Output file, if omitted the output will be printed to stdout");

  try
  {
    app.parse(argc, argv);
  }
  catch (const CLI::ParseError& e)
  {
    return (app).exit(e);
  }

  const std::string extension = vtksys::SystemTools::GetFilenameLastExtension(args.filename);

  auto data = vtkSmartPointer<vtkPolyData>::New();

  if (extension == ".vtk")
  {
    vtkNew<vtkPolyDataReader> inputReader;
    inputReader->SetFileName(args.filename.c_str());
    inputReader->Update();
    data = inputReader->GetOutput();
  }
  else if (extension == ".vtp")
  {
    vtkNew<vtkXMLPolyDataReader> inputReader;
    inputReader->SetFileName(args.filename.c_str());
    inputReader->Update();
    data = inputReader->GetOutput();
  }
  else
  {
    std::cerr << "Invalid format ! Neither a .vtk or .vtp file was supplied!" << std::endl;
    return 1;
  }

  if (data != nullptr)
  {
    vtkNJson json = vtk::Serialize(data);

    json["metadata"] = GetMetaData(args.filename);

    std::ofstream out;
    std::streambuf* previous = nullptr;

    if (!args.output.empty())
    {
      // redirect std::cout to file
      out = std::ofstream(args.output);
      previous = std::cout.rdbuf();
      std::cout.rdbuf(out.rdbuf());
    }

    Dump(json, args.format);

    if (previous)
    {
      // redirect std::cout bcak to stdout
      std::cout.rdbuf(previous);
    }
    return 0;
  }
  else
  {
    std::cerr << "Error reading file !" << std::endl;
    return 1;
  }

  return 1;
}
