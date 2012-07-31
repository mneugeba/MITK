/*===================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center,
Division of Medical and Biological Informatics.
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or http://www.mitk.org for details.

===================================================================*/
#include <mitkContourModelMapper2D.h>

#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkProperty.h>
#include <vtkPlane.h>
#include <vtkCutter.h>
#include <vtkStripper.h>
#include <vtkTubeFilter.h>

mitk::ContourModelMapper2D::ContourModelMapper2D()
{
}


mitk::ContourModelMapper2D::~ContourModelMapper2D()
{
}


const mitk::ContourModel* mitk::ContourModelMapper2D::GetInput( void )
{
  //convient way to get the data from the dataNode
  return static_cast< const mitk::ContourModel * >( this->GetData() );
}


vtkProp* mitk::ContourModelMapper2D::GetVtkProp(mitk::BaseRenderer* renderer)
{  
  //return the actor corresponding to the renderer
  return m_LSH.GetLocalStorage(renderer)->m_Actor;
}


void mitk::ContourModelMapper2D::MitkRenderOverlay(BaseRenderer* renderer)
{
  if ( this->IsVisible(renderer)==false )
    return;
  if ( this->GetVtkProp(renderer)->GetVisibility() )
  {
    this->GetVtkProp(renderer)->RenderOverlay(renderer->GetVtkRenderer());
  }
}



void mitk::ContourModelMapper2D::MitkRenderOpaqueGeometry(BaseRenderer* renderer)
{
  if ( this->IsVisible( renderer )==false )
    return;
  if ( this->GetVtkProp(renderer)->GetVisibility() )
  {
    this->GetVtkProp(renderer)->RenderOpaqueGeometry( renderer->GetVtkRenderer() );
  }
}



void mitk::ContourModelMapper2D::MitkRenderTranslucentGeometry(BaseRenderer* renderer)
{
  if ( this->IsVisible(renderer)==false )
    return;
  if ( this->GetVtkProp(renderer)->GetVisibility() )
  {
    this->GetVtkProp(renderer)->RenderTranslucentPolygonalGeometry(renderer->GetVtkRenderer());
  }
}



void mitk::ContourModelMapper2D::MitkRenderVolumetricGeometry(BaseRenderer* renderer)
{
  if(IsVisible(renderer)==false)
    return;
  if ( GetVtkProp(renderer)->GetVisibility() )
  {
    this->GetVtkProp(renderer)->RenderVolumetricGeometry(renderer->GetVtkRenderer());
  }
}



void mitk::ContourModelMapper2D::GenerateDataForRenderer( mitk::BaseRenderer *renderer )
{
  /*++ convert the contour to vtkPolyData and set it as input for our mapper ++*/

  LocalStorage *localStorage = m_LSH.GetLocalStorage(renderer);

  mitk::ContourModel* inputContour  = static_cast< mitk::ContourModel* >( this->GetData() );

  localStorage->m_OutlinePolyData = this->CreateVtkPolyDataFromContour(inputContour, renderer);

  localStorage->m_Mapper->SetInput(localStorage->m_OutlinePolyData);

  this->ApplyContourProperties(renderer);

}



void mitk::ContourModelMapper2D::Update(mitk::BaseRenderer* renderer)
{
  if ( !this->IsVisible( renderer ) )
  {
    return;
  }

  //check if there is something to be rendered
  mitk::ContourModel* data  = static_cast< mitk::ContourModel*>( this->GetData() );
  if ( data == NULL )
  {
    return;
  }

  // Calculate time step of the input data for the specified renderer (integer value)
  this->CalculateTimeStep( renderer );

  // Check if time step is valid
  const TimeSlicedGeometry *dataTimeGeometry = data->GetTimeSlicedGeometry();
  if ( ( dataTimeGeometry == NULL )
    || ( dataTimeGeometry->GetTimeSteps() == 0 )
    || ( !dataTimeGeometry->IsValidTime( this->GetTimestep() ) ) )
  {
    return;
  }

  const DataNode *node = this->GetDataNode();
  data->UpdateOutputInformation();
  LocalStorage *localStorage = m_LSH.GetLocalStorage(renderer);

  //check if something important has changed and we need to rerender
  if ( (localStorage->m_LastUpdateTime < node->GetMTime()) //was the node modified?
    || (localStorage->m_LastUpdateTime < data->GetPipelineMTime()) //Was the data modified?
    || (localStorage->m_LastUpdateTime < renderer->GetCurrentWorldGeometry2DUpdateTime()) //was the geometry modified?
    || (localStorage->m_LastUpdateTime < renderer->GetCurrentWorldGeometry2D()->GetMTime())
    || (localStorage->m_LastUpdateTime < node->GetPropertyList()->GetMTime()) //was a property modified?
    || (localStorage->m_LastUpdateTime < node->GetPropertyList(renderer)->GetMTime()) )
  {
    this->GenerateDataForRenderer( renderer );
  }

  // since we have checked that nothing important has changed, we can set
  // m_LastUpdateTime to the current time
  localStorage->m_LastUpdateTime.Modified();
}



vtkSmartPointer<vtkPolyData> mitk::ContourModelMapper2D::CreateVtkPolyDataFromContour(mitk::ContourModel* inputContour, mitk::BaseRenderer* renderer)
{
  /* First of all convert the control points of the contourModel to vtk points
   * and add lines in between them
   */
  vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New(); //the points to draw
  vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New(); //the lines to connect the points

  //iterate over all control points
  mitk::ContourModel::VertexIterator current = inputContour->IteratorBegin();
  mitk::ContourModel::VertexIterator next = inputContour->IteratorBegin();
  next++;

  mitk::ContourModel::VertexIterator end = inputContour->IteratorEnd();

  while(next != end)
  {
    mitk::ContourModel::VertexType* currentControlPoint = *current;
    mitk::ContourModel::VertexType* nextControlPoint = *next;

    vtkIdType p1 = points->InsertNextPoint(currentControlPoint->Coordinates[0], currentControlPoint->Coordinates[1], currentControlPoint->Coordinates[2]);
    vtkIdType p2 = points->InsertNextPoint(nextControlPoint->Coordinates[0], nextControlPoint->Coordinates[1], nextControlPoint->Coordinates[2]);
    //add the line between both contorlPoints
    lines->InsertNextCell(2);
    lines->InsertCellPoint(p1);
    lines->InsertCellPoint(p2);

    current++; 
    next++;
  }

  /* If the contour is closed an additional line has to be created between the very first point
   * and the last point
   */
  if(inputContour->IsClosed())
  {
    //add a line from the last to the first control point
    mitk::ContourModel::VertexType* firstControlPoint = *(inputContour->IteratorBegin());
    mitk::ContourModel::VertexType* lastControlPoint = *(--(inputContour->IteratorEnd()));
    vtkIdType p2 = points->InsertNextPoint(lastControlPoint->Coordinates[0], lastControlPoint->Coordinates[1], lastControlPoint->Coordinates[2]);
    vtkIdType p1 = points->InsertNextPoint(firstControlPoint->Coordinates[0], firstControlPoint->Coordinates[1], firstControlPoint->Coordinates[2]);

    //add the line between both contorlPoints
    lines->InsertNextCell(2);
    lines->InsertCellPoint(p1);
    lines->InsertCellPoint(p2);
  }

  // Create a polydata to store everything in
  vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
  // Add the points to the dataset
  polyData->SetPoints(points);
  // Add the lines to the dataset
  polyData->SetLines(lines);

  //check for the worldgeometry from the current render window
  mitk::PlaneGeometry* currentWorldGeometry = dynamic_cast<mitk::PlaneGeometry*>( const_cast<mitk::Geometry2D*>(renderer->GetCurrentWorldGeometry2D()));
  if (currentWorldGeometry)
  {
    //slice through the data to get a 2D representation of the (possible) 3D contour

    //needed because currently there is no outher solution if the contour is within the plane
    vtkSmartPointer<vtkTubeFilter> tubeFilter = vtkSmartPointer<vtkTubeFilter>::New();
    tubeFilter->SetInput(polyData);
    tubeFilter->SetRadius(0.01);

    //origin and normal of vtkPlane
    mitk::Point3D origin = currentWorldGeometry->GetOrigin();
    mitk::Vector3D normal = currentWorldGeometry->GetNormal();

    //the implicit function to slice through the polyData
    vtkSmartPointer<vtkPlane> plane = vtkSmartPointer<vtkPlane>::New();
    plane->SetOrigin(origin[0], origin[1], origin[2]);
    plane->SetNormal(normal[0], normal[1], normal[2]);

    //cuts through vtkPolyData with a given implicit function. In our case a plane
    vtkSmartPointer<vtkCutter> cutter = vtkSmartPointer<vtkCutter>::New();

    cutter->SetCutFunction(plane);
    //cutter->SetInput(polyData);
    cutter->SetInputConnection(tubeFilter->GetOutputPort());


    vtkSmartPointer<vtkStripper> cutStrips = vtkStripper::New();

    cutStrips->SetInputConnection(cutter->GetOutputPort()); 

    cutStrips->Update();


    //store the result in a new polyData
    vtkSmartPointer<vtkPolyData> cutPolyData = vtkSmartPointer<vtkPolyData>::New();

    cutPolyData= cutStrips->GetOutput();

    return cutPolyData;
  }
  
  return polyData;
}



void mitk::ContourModelMapper2D::ApplyContourProperties(mitk::BaseRenderer* renderer)
{
  LocalStorage *localStorage = m_LSH.GetLocalStorage(renderer);

  float binaryOutlineWidth(1.0);
  if (this->GetDataNode()->GetFloatProperty( "contour width", binaryOutlineWidth, renderer ))
  {
    localStorage->m_Actor->GetProperty()->SetLineWidth(binaryOutlineWidth);
  }

  localStorage->m_Actor->GetProperty()->SetColor(0.9, 1.0, 0.1);
}


/*+++++++++++++++++++ LocalStorage part +++++++++++++++++++++++++*/

mitk::ContourModelMapper2D::LocalStorage* mitk::ContourModelMapper2D::GetLocalStorage(mitk::BaseRenderer* renderer)
{
  return m_LSH.GetLocalStorage(renderer);
}


mitk::ContourModelMapper2D::LocalStorage::LocalStorage()
{
  m_Mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  m_Actor = vtkSmartPointer<vtkActor>::New();
  m_OutlinePolyData = vtkSmartPointer<vtkPolyData>::New();

  //set the mapper for the actor
  m_Actor->SetMapper(m_Mapper);
}


void mitk::ContourModelMapper2D::SetDefaultProperties(mitk::DataNode* node, mitk::BaseRenderer* renderer, bool overwrite)
{
  node->AddProperty( "color", ColorProperty::New(1.0,0.0,0.0), renderer, overwrite );
  node->AddProperty( "contour width", mitk::FloatProperty::New( 1.0 ), renderer, overwrite );

  Superclass::SetDefaultProperties(node, renderer, overwrite);
}