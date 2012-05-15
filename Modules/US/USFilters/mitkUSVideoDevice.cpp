/*=========================================================================

Program:   Medical Imaging & Interaction Toolkit
Language:  C++
Date:      $Date$
Version:   $Revision$

Copyright (c) German Cancer Research Center, Division of Medical and
Biological Informatics. All rights reserved.
See MITKCopyright.txt or http://www.mitk.org/copyright.html for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#include "mitkUSVideoDevice.h"
#include <string>


mitk::USVideoDevice::USVideoDevice(int videoDeviceNumber, std::string manufacturer, std::string model) : mitk::USDevice(manufacturer, model, true)
{
  this->SetNumberOfInputs(1);
  this->SetNumberOfOutputs(1);
  m_Source = mitk::USImageVideoSource::New();
  m_Source->SetCameraInput(videoDeviceNumber);
}

mitk::USVideoDevice::USVideoDevice(std::string videoFilePath, std::string manufacturer, std::string model) : mitk::USDevice(manufacturer, model, true)
{
  this->SetNumberOfInputs(1);
  this->SetNumberOfOutputs(1);
  m_Source = mitk::USImageVideoSource::New();
  m_Source->SetVideoFileInput(videoFilePath);
}

mitk::USVideoDevice::~USVideoDevice()
{
  
}

void mitk::USVideoDevice::GenerateData()
{
 // MITK_INFO << "Generate called in USVideoDevice!";
  mitk::USImage::Pointer result;
  result = m_Source->GetNextImage();
  
  // Set Metadata
  result->SetMetadata(this->m_Metadata);

  this->SetNthOutput(0, result);
 // MITK_INFO << "Successfully generated Image in USVideoDevice!";
}