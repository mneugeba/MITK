#include "mitkBaseProperty.h"

//##ModelId=3E38FF0902A7
mitk::BaseProperty::BaseProperty() : m_Enabled(true)
{
}


//##ModelId=3E38FF0902CF
mitk::BaseProperty::~BaseProperty()
{
}

std::string mitk::BaseProperty::GetValueAsString() const
{
   return "GetValueAsString not implemented"; 
}
