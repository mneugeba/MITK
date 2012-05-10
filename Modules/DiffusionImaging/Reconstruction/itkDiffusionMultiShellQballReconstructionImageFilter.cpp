/*=========================================================================

Program:   Medical Imaging & Interaction Toolkit
Language:  C++
Date:      $Date: 2009-07-14 19:11:20 +0200 (Tue, 14 Jul 2009) $
Version:   $Revision: 18127 $

Copyright (c) German Cancer Research Center, Division of Medical and
Biological Informatics. All rights reserved.
See MITKCopyright.txt or http://www.mitk.org/copyright.html for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef __itkDiffusionMultiShellQballReconstructionImageFilter_cpp
#define __itkDiffusionMultiShellQballReconstructionImageFilter_cpp

#include <itkDiffusionMultiShellQballReconstructionImageFilter.h>
#include <itkImageRegionConstIterator.h>
#include <itkImageRegionConstIteratorWithIndex.h>
#include <itkImageRegionIterator.h>
#include <itkArray.h>
#include <vnl/vnl_vector.h>

#include <boost/version.hpp>
#include <stdio.h>
#include <locale>
#include <strstream>

#define _USE_MATH_DEFINES
#include <math.h>

#include "mitkSphericalHarmonicsFunctions.h"
#include "mitkVNLVectorFunctions.h"
#include "itkPointShell.h"
#include <memory>


namespace itk {

#define QBALL_ANAL_RECON_PI       M_PI

template< class T, class TG, class TO, int L, int NODF>
DiffusionMultiShellQballReconstructionImageFilter<T,TG,TO,L,NODF>
::DiffusionMultiShellQballReconstructionImageFilter() :
  m_GradientDirectionContainer(NULL),
  m_NumberOfGradientDirections(0),
  m_NumberOfBaselineImages(1),
  m_Threshold(NumericTraits< ReferencePixelType >::NonpositiveMin()),
  m_BValue(1.0),
  m_Lambda(0.0),
  m_IsHemisphericalArrangementOfGradientDirections(false),
  m_IsArithmeticProgession(false),
  m_ReconstructionType(Standard1Shell)
{
  // At least 1 inputs is necessary for a vector image.
  // For images added one at a time we need at least six
  this->SetNumberOfRequiredInputs( 1 );
}


template< class TReferenceImagePixelType, class TGradientImagePixelType, class TOdfPixelType, int NOrderL, int NrOdfDirections>
typename itk::DiffusionMultiShellQballReconstructionImageFilter< TReferenceImagePixelType,TGradientImagePixelType,TOdfPixelType, NOrderL,NrOdfDirections>::OdfPixelType itk::DiffusionMultiShellQballReconstructionImageFilter<TReferenceImagePixelType, TGradientImagePixelType, TOdfPixelType, NOrderL, NrOdfDirections>
::Normalize( OdfPixelType odf, typename NumericTraits<ReferencePixelType>::AccumulateType b0 )
{
  for(int i=0; i<NrOdfDirections; i++)
  {
    odf[i] = odf[i] < 0 ? 0 : odf[i];
    odf[i] *= QBALL_ANAL_RECON_PI*4/NrOdfDirections;
  }
  TOdfPixelType sum = 0;
  for(int i=0; i<NrOdfDirections; i++)
  {
    sum += odf[i];
  }
  if(sum>0)
    odf /= sum;

  return odf;
}

template<class TReferenceImagePixelType, class TGradientImagePixelType, class TOdfPixelType, int NOrderL, int NrOdfDirections>
void itk::DiffusionMultiShellQballReconstructionImageFilter<TReferenceImagePixelType, TGradientImagePixelType, TOdfPixelType,NOrderL, NrOdfDirections>
::Threshold(vnl_vector<double> & vec, double sigma)
{
  for(int i = 0; i < vec.size(); i++)
  {
    if(vec[i] < 0)
    {
      vec[i] = sigma / 2 ;
    }
    if(0 <= vec[i] && vec[i] < sigma )
    {
      vec[i] = sigma / 2 + (vec[i] * vec[i]) / (2 * sigma);
    }
    if( 1 - sigma <= vec[i] && vec[i] < 1 )
    {
      vec[i] = 1-(sigma/2) - ((( 1 - vec[i]) * (1 - vec[i] )) / (2 * sigma) );
    }
    if( 1 <= vec[i] )
    {
      vec[i] = 1 - (sigma / 2);
    }
  }
}

template<class TReferenceImagePixelType, class TGradientImagePixelType, class TOdfPixelType, int NOrderL, int NrOdfDirections>
void itk::DiffusionMultiShellQballReconstructionImageFilter<TReferenceImagePixelType, TGradientImagePixelType, TOdfPixelType,NOrderL, NrOdfDirections>
::Threshold(vnl_matrix<double> & mat, double sigma)
{
  for(int i = 0; i < mat.rows(); i++)
  {
    for( int j = 0; j < mat.cols(); j++ ){

      if(mat(i,j) < 0)
      {
        mat(i,j) = sigma / 2 ;
      }
      if(0 <= mat(i,j) && mat(i,j) < sigma )
      {
        mat(i,j) = sigma / 2 + (mat(i,j) * mat(i,j)) / (2 * sigma);
      }
      if( 1 - sigma <= mat(i,j) && mat(i,j) < 1 )
      {
        mat(i,j) = 1-(sigma/2) - (((1 - mat(i,j)) *( 1 - mat(i,j)))  / (2 * sigma) );
      }
      if( 1 <= mat(i,j) )
      {
        mat(i,j) = 1 - (sigma / 2);
      }
    }
  }
}

template<class TReferenceImagePixelType, class TGradientImagePixelType, class TOdfPixelType, int NOrderL, int NrOdfDirections>
void itk::DiffusionMultiShellQballReconstructionImageFilter<TReferenceImagePixelType, TGradientImagePixelType, TOdfPixelType,NOrderL, NrOdfDirections>
::Projection1( vnl_matrix<double> & E, double delta )
{

  const double sF = sqrt(5.0);

  vnl_vector<double> vOnes(E.rows());
  vOnes.fill(1.0);

  vnl_matrix<double> T0(E.rows(), E.cols());
  T0.fill(0.0);
  vnl_matrix<unsigned char> C(E.rows(), 7);
  C.fill(0);
  vnl_matrix<double> A(E.rows(), 7);
  A.fill(0.0);
  vnl_matrix<double> B(E.rows(), 7);
  B.fill(0.0);

  vnl_vector<double> s0(E.rows());
  s0.fill(0.0);
  vnl_vector<double> a0(E.rows());
  a0.fill(0.0);
  vnl_vector<double> b0(E.rows());
  b0.fill(0.0);
  vnl_vector<double> ta(E.rows());
  ta.fill(0.0);
  vnl_vector<double> tb(E.rows());
  tb.fill(0.0);
  vnl_vector<double> e(E.rows());
  e.fill(0.0);
  vnl_vector<double> m(E.rows());
  m.fill(0.0);
  vnl_vector<double> a(E.rows());
  a.fill(0.0);
  vnl_vector<double> b(E.rows());
  b.fill(0.0);

  // logarithmierung aller werte in E
  for(int i = 0 ; i < E.rows(); i++)
  {
    for(int j = 0 ; j < E.cols(); j++)
    {
      T0(i,j) = -log(E(i,j));
    }
  }


  //T0 = -T0.apply(std::log);

  // Summeiere Zeilenweise über alle Shells sum = E1+E2+E3
  for(int i = 0 ; i < E.rows(); i++)
  {
    s0[i] = T0(i,0) + T0(i,1) + T0(i,2);
  }


  for(int i = 0; i < E.rows(); i ++)
  {
    // Alle Signal-Werte auf der Ersten shell E(N,0) normiert auf s0
    a0 = E(i,0) / s0[i];
    // Alle Signal-Werte auf der Zweiten shell E(N,1) normiert auf s0
    b0 = E(i,1) / s0[i];
  }

  ta = a0 * 3.0;
  tb = b0 * 3.0;
  e = tb - (ta * 2.0);
  m = (tb *  2.0 ) + ta;

  for(int i = 0; i < E.rows(); i++)
  {
    C(i,0) = tb[i] < 1+3*delta && 0.5+1.5*(sF+1)*delta < ta[i] && ta[i] < 1-3* (sF+2) *delta;
    C(i,1) = e[i] <= -1 +3*(2*sF+5)* delta && ta[i] >= 1-3*(sF+2)*delta;
    C(i,2) = m[i] > 3 -3*sF*delta && -1+3*(2*sF+5)*delta < e[i] && e[i]<-3*sF*delta;
    C(i,3) = m[i] >= 3-3*sF*delta && e[i] >= -3 *sF * delta;
    C(i,4) = 2.5 + 1.5*(5+sF)*delta < m[i] && m[i] < 3-3*sF*delta && e[i] > -3*sF*delta;
    C(i,5) = ta[i] <= 0.5+1.5 *(sF+1)*delta && m[i] <= 2.5 + 1.5 *(5+sF) * delta;
    C(i,6) = !( C(i,0) || C(i,1) || C(i,2) || C(i,3) || C(i,4) || C(i,5) ); // ~ANY(C(i,[0-5] ),2)

    A(i,0)=(bool)C(i,0) * a0(i);
    A(i,1)=(bool)C(i,1) * (1.0/3.0-(sF+2)*delta);
    A(i,2)=(bool)C(i,2) * (0.2+0.8*a0(i)-0.4*b0(i)-delta/sF);
    A(i,3)=(bool)C(i,3) * (0.2+delta/sF);
    A(i,4)=(bool)C(i,4) * (0.2*a0(i)+0.4*b0(i)+2*delta/sF);
    A(i,5)=(bool)C(i,5) * (1.0/6.0+0.5*(sF+1)*delta);
    A(i,6)=(bool)C(i,6) * a0(i);

    B(i,0)=(bool)C(i,0) * (1.0/3.0+delta);
    B(i,1)=(bool)C(i,1) * (1.0/3.0+delta);
    B(i,2)=(bool)C(i,2) * (0.4-0.4*a0(i)+0.2*b0(i)-2*delta/sF);
    B(i,3)=(bool)C(i,3) * (0.4-3*delta/sF);
    B(i,4)=(bool)C(i,4) * (0.4*a0(i)+0.8*b0(i)-delta/sF);
    B(i,5)=(bool)C(i,5) * (1.0/3.0+delta);
    B(i,6)=(bool)C(i,6) * b0(i);
  }



  for(int i = 0 ; i < E.rows(); i++)
  {
    double sumA = 0;
    double sumB = 0;
    for(int j = 0 ; j < 7; j++)
    {
      sumA += A(i,j);
      sumB += B(i,j);
    }
    a[i] = sumA;
    b[i] = sumB;
  }

  for(int i = 0; i < E.rows(); i++)
  {
    E(i,0) = exp(-(a[i]*s0[i]));
    E(i,1) = exp(-(b[i]*s0[i]));
    E(i,2) = exp(-((1-a[i]-b[i])*s0[i]));
  }

}




template<class TReferenceImagePixelType, class TGradientImagePixelType, class TOdfPixelType, int NOrderL, int NrOdfDirections>
void itk::DiffusionMultiShellQballReconstructionImageFilter<TReferenceImagePixelType, TGradientImagePixelType, TOdfPixelType,NOrderL, NrOdfDirections>
::Projection2( vnl_vector<double> & A, vnl_vector<double> & a, vnl_vector<double> & b, double delta0)
{

  const double s6 = sqrt(6);
  const double s15 = s6/2.0;

  vnl_vector<double> delta(a.size());
  delta.fill(delta0);

  vnl_matrix<double> AM(a.size(), 15);
  AM.fill(0.0);
  vnl_matrix<double> aM(a.size(), 15);
  aM.fill(0.0);
  vnl_matrix<double> bM(a.size(), 15);
  bM.fill(0.0);

  vnl_matrix<unsigned char> B(a.size(), 15);
  B.fill(0);


  AM.set_column(0, A);
  AM.set_column(1, A);
  AM.set_column(2, A);
  AM.set_column(3, delta);
  AM.set_column(4, (A+a-b - (delta*s6))/3.0);
  AM.set_column(5, delta);
  AM.set_column(6, delta);
  AM.set_column(7, delta);
  AM.set_column(8, A);
  AM.set_column(9, 0.2*(a*2+A-2*(s6+1)*delta));
  AM.set_column(10,0.2*(b*(-2)+A+2-2*(s6+1)*delta));
  AM.set_column(11, delta);
  AM.set_column(12, delta);
  AM.set_column(13, delta);
  AM.set_column(14, 0.5-(1+s15)*delta);


  aM.set_column(0, a);
  aM.set_column(1, a);
  aM.set_column(2, -delta + 1);
  aM.set_column(3, a);
  aM.set_column(4, (A*2+a*5+b+s6*delta)/6.0);
  aM.set_column(5, a);
  aM.set_column(6, -delta + 1);
  aM.set_column(7, 0.5*(a+b)+(1+s15)*delta);
  aM.set_column(8, -delta + 1);
  aM.set_column(9, 0.2*(a*4+A*2+(s6+1)*delta));
  aM.set_column(10, -delta + 1);
  aM.set_column(11, (s6+3)*delta);
  aM.set_column(12, -delta + 1);
  aM.set_column(13, -delta + 1);
  aM.set_column(14, -delta + 1);

  bM.set_column(0, b);
  bM.set_column(1, delta);
  bM.set_column(2, b);
  bM.set_column(3, b);
  bM.set_column(4, (A*(-2)+a+b*5-s6*delta)/6.0);
  bM.set_column(5, delta);
  bM.set_column(6, b);
  bM.set_column(7, 0.5*(a+b)-(1+s15)*delta);
  bM.set_column(8, delta);
  bM.set_column(9, delta);
  bM.set_column(10, 0.2*(b*4-A*2+1-(s6+1)*delta));
  bM.set_column(11, delta);
  bM.set_column(12, delta);
  bM.set_column(13, -delta*(s6+3) + 1);
  bM.set_column(14, delta);

  delta0 *= 0.99;

  for(int i = 0 ; i < a.size(); i ++)
  {
    for(int j = 0 ; j < 15; j ++)
    {
      B(i,j) = delta0 < AM(i,j) && 2 * (AM(i,j) + delta0 * s15) < aM(i,j) - bM(i,j) && bM(i,j) > delta0 && aM(i,j) < 1- delta0;
    }
  }

  vnl_matrix<double> R2(a.size(), 15);
  R2.fill(0.0);
  vnl_matrix<double> A_(a.size(), 15);
  A_.fill(0.0);
  vnl_matrix<double> a_(a.size(), 15);
  a_.fill(0.0);
  vnl_matrix<double> b_(a.size(), 15);
  b_.fill(0.0);



  vnl_matrix<double> OnesVecMat(1, 15);
  OnesVecMat.fill(1.0);

  vnl_matrix<double> AVecMat(a.size(), 1);
  AVecMat.set_column(0,A);

  vnl_matrix<double> aVecMat(a.size(), 1);
  aVecMat.set_column(0,a);

  vnl_matrix<double> bVecMat(a.size(), 1);
  bVecMat.set_column(0,b);

  A_ = AM - (AVecMat * OnesVecMat);
  a_ = aM - (aVecMat * OnesVecMat);
  b_ = bM - (bVecMat * OnesVecMat);

  for(int i = 0 ; i < a.size(); i++)
    for(int j = 0 ; j < 15; j++)
    {
      A_(i,j) *= A_(i,j);
      a_(i,j) *= a_(i,j);
      b_(i,j) *= b_(i,j);
    }


  R2 = A_ + a_ + b_;

  for(int i = 0 ; i < a.size(); i ++)
  {
    for(int j = 0 ; j < 15; j ++)
    {
      if(B(i,j) == 0) R2(i,j) = 1e20;
    }
  }

  std::vector<unsigned int> indicies(a.size());

  // suche den spalten-index der zu der kleinsten Zahl einer Zeile korrespondiert
  for(int i = 0 ; i < a.size(); i++)
  {
    unsigned int index = 0;
    double minvalue = 999;
    for(int j = 0 ; j < 15 ; j++)
    {
      if(R2(i,j) < minvalue){
        minvalue = R2(i,j);
        index = j;
      }
    }
    indicies[i] = index;
  }

  for(int i = 0 ; i < a.size(); i++)
  {
    A[i] = AM(i,indicies[i]);
    a[i] = aM(i,indicies[i]);
    b[i] = bM(i,indicies[i]);
  }
}


template<class TReferenceImagePixelType, class TGradientImagePixelType, class TOdfPixelType, int NOrderL, int NrOdfDirections>
void itk::DiffusionMultiShellQballReconstructionImageFilter<TReferenceImagePixelType, TGradientImagePixelType, TOdfPixelType,NOrderL, NrOdfDirections>
::S_S0Normalization( vnl_vector<double> & vec, typename NumericTraits<ReferencePixelType>::AccumulateType b0 )
{

  double b0f = (double)b0;
  for(int i = 0; i < vec.size(); i++)
  {
    if (b0f==0)
      b0f = 0.01;
    if(vec[i] >= b0f)
      vec[i] = b0f - 0.001;
    vec[i] /= b0f;
  }

}


template<class TReferenceImagePixelType, class TGradientImagePixelType, class TOdfPixelType, int NOrderL, int NrOdfDirections>
void itk::DiffusionMultiShellQballReconstructionImageFilter<TReferenceImagePixelType, TGradientImagePixelType, TOdfPixelType,NOrderL, NrOdfDirections>
::S_S0Normalization( vnl_matrix<double> & mat, typename NumericTraits<ReferencePixelType>::AccumulateType b0 )
{
  double b0f = (double)b0;
  for(int i = 0; i < mat.rows(); i++)
  {
    for( int j = 0; j < mat.cols(); j++ ){
      if (b0f==0)
        b0f = 0.01;
      if(mat(i,j) >= b0f)
        mat(i,j) = b0f - 0.001;
      mat(i,j) /= b0f;
    }
  }
}



template< class T, class TG, class TO, int L, int NODF>
void DiffusionMultiShellQballReconstructionImageFilter<T,TG,TO,L,NODF>
::DoubleLogarithm(vnl_vector<double> & vec)
{
  for(int i = 0; i < vec.size(); i++)
  {
    vec[i] = log(-log(vec[i]));
  }
}

template< class T, class TG, class TO, int L, int NODF>
void DiffusionMultiShellQballReconstructionImageFilter<T,TG,TO,L,NODF>
::SetGradientImage( GradientDirectionContainerType *gradientDirection, const GradientImagesType *gradientImage )
{
  this->m_GradientDirectionContainer = gradientDirection;
  this->m_NumberOfBaselineImages = 0;

  for(GradientDirectionContainerType::Iterator it = gradientDirection->Begin(); it != gradientDirection->End(); it++)
  {
    if( it.Value().one_norm() <= 0.0 )
      this->m_NumberOfBaselineImages ++;
    else
      it.Value() = it.Value() / it.Value().two_norm();
  }

  this->m_NumberOfGradientDirections = gradientDirection->Size() - this->m_NumberOfBaselineImages;

  // ensure that the gradient image we received has as many components as
  // the number of gradient directions
  if( gradientImage->GetVectorLength() != this->m_NumberOfBaselineImages + m_NumberOfGradientDirections )
  {
    itkExceptionMacro( << m_NumberOfGradientDirections << " gradients + " << this->m_NumberOfBaselineImages
                       << "baselines = " << m_NumberOfGradientDirections + this->m_NumberOfBaselineImages
                       << " directions specified but image has " << gradientImage->GetVectorLength()
                       << " components.");
  }

  this->ProcessObject::SetNthInput( 0, const_cast< GradientImagesType* >(gradientImage) );
}

template< class T, class TG, class TO, int L, int NODF>
void DiffusionMultiShellQballReconstructionImageFilter<T,TG,TO,L,NODF>
::BeforeThreadedGenerateData()
{
  itk::TimeProbe clock;
  clock.Start();

  if( m_NumberOfGradientDirections < (((L+1)*(L+2))/2) /* && m_NumberOfGradientDirections < 6 */ )
  {
    itkExceptionMacro( << "At least " << ((L+1)*(L+2))/2 << " gradient directions are required" );
  }
  // Input must be an itk::VectorImage.
  std::string gradientImageClassName(this->ProcessObject::GetInput(0)->GetNameOfClass());

  if ( strcmp(gradientImageClassName.c_str(),"VectorImage") != 0 )
    itkExceptionMacro( << "There is only one Gradient image. I expect that to be a VectorImage. But its of type: " << gradientImageClassName );

  m_BZeroImage = BZeroImageType::New();
  typename GradientImagesType::Pointer img = static_cast< GradientImagesType * >( this->ProcessObject::GetInput(0) );
  m_BZeroImage->SetSpacing( img->GetSpacing() );   // Set the image spacing
  m_BZeroImage->SetOrigin( img->GetOrigin() );     // Set the image origin
  m_BZeroImage->SetDirection( img->GetDirection() );  // Set the image direction
  m_BZeroImage->SetLargestPossibleRegion( img->GetLargestPossibleRegion());
  m_BZeroImage->SetBufferedRegion( img->GetLargestPossibleRegion() );
  m_BZeroImage->Allocate();

  // if no GradientIndexMap is set take all Gradients to GradientIndicies-Vector and add this to GradientIndexMap[ALL]
  // add All Bzero
  if(m_GradientIndexMap.size() == 0){

    // split for all gradient directions the image in baseline-indicies and gradient-indicies for a fast access
    GradientDirectionContainerType::ConstIterator gdcit;
    for( gdcit = this->m_GradientDirectionContainer->Begin(); gdcit != this->m_GradientDirectionContainer->End(); ++gdcit)
    {
      if(gdcit.Value().one_norm() <= 0.0) // if vector length at current position is 0, it is a basline signal
      {
        m_GradientIndexMap[0].push_back(gdcit.Index());
      }else{ // it is a gradient signal of length != 0
        m_GradientIndexMap[1].push_back(gdcit.Index());
      }
    }
    m_ReconstructionType = Standard1Shell;
  }else if(m_GradientIndexMap.size() == 4){

    GradientIndexMapIteraotr it = m_GradientIndexMap.begin();
    it++;
    int b1 = (*it).first;
    it++;
    int b2 = (*it).first;
    it++;
    int b3 = (*it).first;


    if(b2 - b1 == b1 && b3 - b2 == b1 )
    {
      m_ReconstructionType = Analytical3Shells;
    }else
    {
      m_ReconstructionType = NumericalNShells;
    }
  }else if(m_GradientIndexMap.size() > 2)
  {
    m_ReconstructionType = NumericalNShells;
  }



  switch(m_ReconstructionType)
  {
  case Analytical3Shells:
  {
    GradientIndexMapIteraotr it = m_GradientIndexMap.begin();
    it++;
    this->ComputeReconstructionMatrix((*it).second);
    break;
  }
  case NumericalNShells:
    this->ComputeReconstructionMatrix(m_GradientIndexMap[1]);
    break;
  case Standard1Shell:
    this->ComputeReconstructionMatrix(m_GradientIndexMap[1]);
    break;
  }

  clock.Stop();
  MITK_INFO << "Before GenerateData : " << clock.GetTotal();

}



template< class T, class TG, class TO, int L, int NODF>
void DiffusionMultiShellQballReconstructionImageFilter<T,TG,TO,L,NODF>
::StandardOneShellReconstruction(const OutputImageRegionType& outputRegionForThread)
{
  // Get output image pointer
  typename OutputImageType::Pointer outputImage = static_cast< OutputImageType * >(this->ProcessObject::GetOutput(0));

  // ImageRegionIterator for the output image
  ImageRegionIterator< OutputImageType > oit(outputImage, outputRegionForThread);
  oit.GoToBegin();

  // ImageRegionIterator for the BZero (output) image
  ImageRegionIterator< BZeroImageType > oit2(m_BZeroImage, outputRegionForThread);
  oit2.GoToBegin();

  IndiciesVector BZeroIndicies = m_GradientIndexMap[0];
  IndiciesVector SignalIndicies = m_GradientIndexMap[1];

  // if the gradient directiosn aragement is hemispherical, duplicate all gradient directions
  // alone, interested in the value, the direction can be neglected
  if(m_IsHemisphericalArrangementOfGradientDirections){
    int NumbersOfGradientIndicies = SignalIndicies.size();
    for (int i = 0 ; i < NumbersOfGradientIndicies; i++)
      SignalIndicies.push_back(SignalIndicies[i]);
  }


  // Get input gradient image pointer
  typename GradientImagesType::Pointer gradientImagePointer = static_cast< GradientImagesType * >( this->ProcessObject::GetInput(0) );

  //  Const ImageRegionIterator for input gradient image
  typedef ImageRegionConstIterator< GradientImagesType > GradientIteratorType;
  GradientIteratorType git(gradientImagePointer, outputRegionForThread );
  git.GoToBegin();

  typedef typename GradientImagesType::PixelType         GradientVectorType;

  // iterate overall voxels of the gradient image region
  while( ! git.IsAtEnd() )
  {
    GradientVectorType b = git.Get();

    // compute the average bzero signal
    typename NumericTraits<ReferencePixelType>::AccumulateType b0 = NumericTraits<ReferencePixelType>::Zero;
    for(unsigned int i = 0; i < BZeroIndicies.size(); ++i)
    {
      b0 += b[BZeroIndicies[i]];
    }
    b0 /= BZeroIndicies.size();

    // ODF Vector
    OdfPixelType odf(0.0);

    // Create the Signal Vector
    vnl_vector<double> SignalVector(m_NumberOfGradientDirections);
    if( (b0 != 0) && (b0 >= m_Threshold) )
    {

      for( unsigned int i = 0; i< SignalIndicies.size(); i++ )
      {
        SignalVector[i] = static_cast<double>(b[SignalIndicies[i]]);
      }

      // apply threashold an generate ln(-ln(E)) signal
      // Replace SignalVector with PreNormalized SignalVector
      S_S0Normalization(SignalVector, b0);
      DoubleLogarithm(SignalVector);

      // ODF coeffs-vector
      vnl_vector<double> coeffs(m_NumberCoefficients);

      // approximate ODF coeffs
      coeffs = ( (*m_CoeffReconstructionMatrix) * SignalVector );
      coeffs[0] += 1.0/(2.0*sqrt(QBALL_ANAL_RECON_PI));

      odf = mitk::mitk_vnl_function::element_cast<double, TO>(( (*m_ODFSphericalHarmonicBasisMatrix) * coeffs )).data_block();
    }
    // set ODF to ODF-Image
    oit.Set( odf );
    ++oit;

    // set b0(average) to b0-Image
    oit2.Set( b0 );
    ++oit2;

    ++git;

  }

  MITK_INFO << "One Thread finished reconstruction";
}

#include <itkTimeProbe.h>

template< class T, class TG, class TO, int L, int NODF>
void DiffusionMultiShellQballReconstructionImageFilter<T,TG,TO,L,NODF>
::AnalyticalThreeShellReconstruction(const OutputImageRegionType& outputRegionForThread)
{


  typename OutputImageType::Pointer outputImage = static_cast< OutputImageType * >(this->ProcessObject::GetOutput(0));

  ImageRegionIterator< OutputImageType > oit(outputImage, outputRegionForThread);
  oit.GoToBegin();

  ImageRegionIterator< BZeroImageType > oit2(m_BZeroImage, outputRegionForThread);
  oit2.GoToBegin();

  IndiciesVector BZeroIndicies = m_GradientIndexMap[0];

  //MAP-Pair(B-value, SignalShell1Indiecies)
  GradientIndexMapIteraotr it = m_GradientIndexMap.begin();
  // it = b-vlaue = 0

  it++;
  // it = b-value = 1000
  IndiciesVector Shell1Indiecies = (*it).second;

  it++;
  // it = b-value = 2000
  IndiciesVector Shell2Indiecies = (*it).second;

  it++;
  // it = b-value = 3000
  IndiciesVector Shell3Indiecies = (*it).second;

  if(m_IsHemisphericalArrangementOfGradientDirections){
    int NumbersOfGradientIndicies = Shell1Indiecies.size();
    for (int i = 0 ; i < NumbersOfGradientIndicies; i++){
      Shell1Indiecies.push_back(Shell1Indiecies[i]);
      Shell2Indiecies.push_back(Shell2Indiecies[i]);
      Shell3Indiecies.push_back(Shell3Indiecies[i]);
    }
  }

  // Get input gradient image pointer
  typename GradientImagesType::Pointer gradientImagePointer = static_cast< GradientImagesType * >( this->ProcessObject::GetInput(0) );

  //  Const ImageRegionIterator for input gradient image
  typedef ImageRegionConstIterator< GradientImagesType > GradientIteratorType;
  GradientIteratorType git(gradientImagePointer, outputRegionForThread );
  git.GoToBegin();

  typedef typename GradientImagesType::PixelType         GradientVectorType;

  //------------------------- Preperations Zone ------------------------------------

  // N x 3

  //  x E1 , E2 , E3
  //
  //  N :    :    :
  //    :    :    :
  //
  vnl_matrix< double > * E = new vnl_matrix<double>(Shell1Indiecies.size(), 3);
  vnl_vector<double> * AlphaValues = new vnl_vector<double>(Shell1Indiecies.size());
  vnl_vector<double> * BetaValues = new vnl_vector<double>(Shell1Indiecies.size());
  vnl_vector<double> * LAValues = new vnl_vector<double>(Shell1Indiecies.size());
  vnl_vector<double> * PValues = new vnl_vector<double>(Shell1Indiecies.size());
  OdfPixelType odf(0.0);
  //------------------------- Preperations Zone END ---------------------------------

  // iterate overall voxels of the gradient image region
  while( ! git.IsAtEnd() )
  {
    GradientVectorType b = git.Get();

    // compute the average bzero signal
    typename NumericTraits<ReferencePixelType>::AccumulateType b0 = NumericTraits<ReferencePixelType>::Zero;
    for(unsigned int i = 0; i < BZeroIndicies.size(); ++i)
    {
      b0 += b[BZeroIndicies[i]];
    }
    b0 /= BZeroIndicies.size();

    if( (b0 != 0) && (b0 >= m_Threshold) )
    {

      for(int i = 0 ; i < Shell1Indiecies.size(); i++)
      {
        E->put(i,0, static_cast<double>(b[Shell1Indiecies[i]]));
        E->put(i,1, static_cast<double>(b[Shell2Indiecies[i]]));
        E->put(i,2, static_cast<double>(b[Shell3Indiecies[i]]));
      }

      //Approximated-Signal by SH fit - using the specific shell directions and values
      E->set_column(1, (*m_SHBasisMatrix) * ((*m_SignalReonstructionMatrix) * (E->get_column(1))));
      E->set_column(2, (*m_SHBasisMatrix) * ((*m_SignalReonstructionMatrix) * (E->get_column(2))));

      // Si/S0
      S_S0Normalization(*E,b0);

      //Implements Eq. [19] and Fig. 4.
      Threshold(*E);

      //inqualities [31]. Taking the lograithm of th first tree inqualities
      //convert the quadratic inqualities to linear ones.
      Projection1(*E);

      double E1, E2, E3, P2,A,B2,B,P,alpha,beta,lambda, ER1, ER2;

      for( unsigned int i = 0; i< Shell1Indiecies.size(); i++ )
      {

        E1 = E->get(i,0);
        E2 = E->get(i,1);
        E3 = E->get(i,2);

        P2 = E2-E1*E1;
        A = (E3 -E1*E2) / ( 2* P2);
        B2 = A * A -(E1 * E3 - E2 * E2) /P2;
        B = 0;
        if(B2 > 0) B = sqrt(B2);

        P = 0;
        if(P2 > 0) P = sqrt(P2);

        alpha = A + B;
        beta = A - B;


        lambda = 0.5 + 0.5 * std::sqrt(1 - std::pow((P * 2 ) / (alpha - beta), 2));;
        ER1 = std::fabs(lambda * (alpha - beta) + (beta - E1 ))
            + std::fabs(lambda * (std::pow(alpha, 2) - std::pow(beta, 2)) + (std::pow(beta, 2) - E2 ))
            + std::fabs(lambda * (std::pow(alpha, 3) - std::pow(beta, 3)) + (std::pow(beta, 3) - E3 ));
        ER2 = std::fabs((lambda-1) * (alpha - beta) + (beta - E1 ))
            + std::fabs((lambda-1) * (std::pow(alpha, 2) - std::pow(beta, 2)) + (std::pow(beta, 2) - E2 ))
            + std::fabs((lambda-1) * (std::pow(alpha, 3) - std::pow(beta, 3)) + (std::pow(beta, 3) - E3 ));


        PValues->put(i, P);
        AlphaValues->put(i, alpha);
        BetaValues->put(i, beta);
        LAValues->put(i,(lambda * (ER1 < ER2)) + ((1-lambda) * (ER1 >= ER2)));

      }

      Projection2(*PValues, *AlphaValues, *BetaValues);

      Threshold(*AlphaValues);
      Threshold(*BetaValues);

      DoubleLogarithm(*AlphaValues);
      DoubleLogarithm(*BetaValues);

      //L * A + (1-L) * B = S
      //L * A + B - L * B = S
      //L*(A - B) + B = S

      vnl_vector<double> SignalVector(element_product((*LAValues) , (*AlphaValues)-(*BetaValues)) + (*BetaValues));
      vnl_vector<double> coeffs((*m_CoeffReconstructionMatrix) *SignalVector );

      // the first coeff is a fix value
      coeffs[0] += 1.0/(2.0*sqrt(QBALL_ANAL_RECON_PI));

      // Cast the Signal-Type from double to float for the ODF-Image
      odf = mitk::mitk_vnl_function::element_cast<double, TO>( (*m_ODFSphericalHarmonicBasisMatrix) * coeffs ).data_block();
      odf *= (QBALL_ANAL_RECON_PI*4/NODF);
      Normalize(odf,b0);
    }

    // set ODF to ODF-Image
    oit.Set( odf );
    ++oit;

    // set b0(average) to b0-Image
    oit2.Set( b0 );
    ++oit2;

    ++git;
  }
  MITK_INFO << "THREAD FINISHED";
  delete E;
  delete AlphaValues;
  delete BetaValues;
  delete PValues;
  delete LAValues;
}


template< class T, class TG, class TO, int L, int NODF>
vnl_vector<TO> DiffusionMultiShellQballReconstructionImageFilter<T,TG,TO,L,NODF>
::AnalyticalThreeShellParameterEstimation(const IndiciesVector * shell1Indicies,const IndiciesVector * shell2Indicies ,const IndiciesVector * shell3Indicies, vnl_vector<TO>)
{



}

template< class T, class TG, class TO, int L, int NODF>
void DiffusionMultiShellQballReconstructionImageFilter<T,TG,TO,L,NODF>
::ThreadedGenerateData(const OutputImageRegionType& outputRegionForThread, int NumberOfThreads)
{
  itk::TimeProbe clock;
  clock.Start();
  switch(m_ReconstructionType)
  {
  case Standard1Shell:
    StandardOneShellReconstruction(outputRegionForThread);
    break;
  case Analytical3Shells:
    AnalyticalThreeShellReconstruction(outputRegionForThread);
    break;
  case NumericalNShells:
    break;
  }
  clock.Stop();
  MITK_INFO << "Reconstruction in : " << clock.GetTotal() << " TU";



}

template< class T, class TG, class TO, int L, int NODF>
void DiffusionMultiShellQballReconstructionImageFilter<T, TG, TO, L, NODF>::
ComputeSphericalHarmonicsBasis(vnl_matrix<double> * QBallReference, vnl_matrix<double> *SHBasisOutput, vnl_matrix<double>* LaplaciaBaltramiOutput, vnl_vector<int>* SHOrderAssociation, vnl_matrix<double>* SHEigenvalues )
{
  for(unsigned int i=0; i< (*SHBasisOutput).rows(); i++)
  {
    for(int k = 0; k <= L; k += 2)
    {
      for(int m =- k; m <= k; m++)
      {
        int j = ( k * k + k + 2 ) / 2 + m - 1;

        // Compute SHBasisFunctions
        double phi = (*QBallReference)(0,i);
        double th = (*QBallReference)(1,i);
        (*SHBasisOutput)(i,j) = mitk::ShericalHarmonicsFunctions::Yj(m,k,th,phi);

        // Laplacian Baltrami Order Association
        if(LaplaciaBaltramiOutput)
          (*LaplaciaBaltramiOutput)(j,j) = k*k*(k + 1)*(k+1);

        // SHEigenvalues with order Accosiation kj
        if(SHEigenvalues)
          (*SHEigenvalues)(j,j) = -k* (k+1);

        // Order Association
        if(SHOrderAssociation)
          (*SHOrderAssociation)[j] = k;

      }
    }
  }
}

template< class T, class TG, class TO, int L, int NODF>
void DiffusionMultiShellQballReconstructionImageFilter<T,TG,TO,L,NODF>
::ComputeFunkRadonTransformationMatrix(vnl_vector<int>* SHOrderAssociationReference, vnl_matrix<double>* FRTMatrixOutput )
{
  for(int i=0; i<m_NumberCoefficients; i++)
  {
    (*FRTMatrixOutput)(i,i) = 2.0 * QBALL_ANAL_RECON_PI * mitk::ShericalHarmonicsFunctions::legendre0((*SHOrderAssociationReference)[i]);
  }
}

template< class T, class TG, class TO, int L, int NOdfDirections>
bool DiffusionMultiShellQballReconstructionImageFilter<T,TG,TO,L,NOdfDirections>
::CheckHemisphericalArrangementOfGradientDirections()
{
  // handle acquisition schemes where only half of the spherical
  // shell is sampled by the gradient directions. In this case,
  // each gradient direction is duplicated in negative direction.
  vnl_vector<double> centerMass(3);
  centerMass.fill(0.0);
  int count = 0;
  GradientDirectionContainerType::ConstIterator gdcit1;
  for( gdcit1 = this->m_GradientDirectionContainer->Begin(); gdcit1 != this->m_GradientDirectionContainer->End(); ++gdcit1)
  {
    if(gdcit1.Value().one_norm() > 0.0)
    {
      centerMass += gdcit1.Value();
      count ++;
    }
  }
  centerMass /= count;
  if(centerMass.two_norm() > 0.1)
  {
    return false;
  }
  return true;
}

template< class T, class TG, class TO, int L, int NOdfDirections>
void DiffusionMultiShellQballReconstructionImageFilter<T,TG,TO,L,NOdfDirections>
::ComputeReconstructionMatrix(std::vector< unsigned int > gradientIndiciesVector)
{

  typedef std::auto_ptr< vnl_matrix< double> >  MatrixDoublePtr;
  typedef std::auto_ptr< vnl_vector< int > >    VectorIntPtr;
  typedef std::auto_ptr< vnl_matrix_inverse< double > >  InverseMatrixDoublePtr;

  int numberOfGradientDirections = gradientIndiciesVector.size();


  if( numberOfGradientDirections < (((L+1)*(L+2))/2) || numberOfGradientDirections < 6  )
  {
    itkExceptionMacro( << "At least (L+1)(L+2)/2 gradient directions are required" );
  }

  CheckDuplicateDiffusionGradients();


  // check if gradient directions are arrangement as a hemisphere(true) or sphere(false)
  m_IsHemisphericalArrangementOfGradientDirections = CheckHemisphericalArrangementOfGradientDirections();
  if(m_IsHemisphericalArrangementOfGradientDirections) numberOfGradientDirections *= 2;


  MatrixDoublePtr Q(new vnl_matrix<double>(3, numberOfGradientDirections));
  Q->fill(0.0);
  // Cartesian to spherical coordinates
  {
    int j = 0;

    for(int i = 0; i < gradientIndiciesVector.size(); i++)
    {

      double x = this->m_GradientDirectionContainer->ElementAt(gradientIndiciesVector[i]).get(0);
      double y = this->m_GradientDirectionContainer->ElementAt(gradientIndiciesVector[i]).get(1);
      double z = this->m_GradientDirectionContainer->ElementAt(gradientIndiciesVector[i]).get(2);
      double cart[3];
      mitk::ShericalHarmonicsFunctions::Cart2Sph(x,y,z,cart);
      (*Q)(0,j) = cart[0];
      (*Q)(1,j) = cart[1];
      (*Q)(2,j++) = cart[2];

    }
    if(m_IsHemisphericalArrangementOfGradientDirections)
    {
      for(int i = 0; i < gradientIndiciesVector.size(); i++)
      {
        double x = this->m_GradientDirectionContainer->ElementAt(gradientIndiciesVector[i]).get(0);
        double y = this->m_GradientDirectionContainer->ElementAt(gradientIndiciesVector[i]).get(1);
        double z = this->m_GradientDirectionContainer->ElementAt(gradientIndiciesVector[i]).get(2);
        double cart[3];
        mitk::ShericalHarmonicsFunctions::Cart2Sph(x,y,z,cart);
        (*Q)(0,j) = cart[0];
        (*Q)(1,j) = cart[1];
        (*Q)(2,j++) = cart[2];
      }
    }
  }

  int LOrder = L;
  m_NumberCoefficients = (int)(LOrder*LOrder + LOrder + 2.0)/2.0 + LOrder;

  m_SHBasisMatrix = new vnl_matrix<double>(numberOfGradientDirections,m_NumberCoefficients);
  m_SHBasisMatrix->fill(0.0);
  VectorIntPtr SHOrderAssociation(new vnl_vector<int>(m_NumberCoefficients));
  SHOrderAssociation->fill(0.0);
  MatrixDoublePtr LaplacianBaltrami(new vnl_matrix<double>(m_NumberCoefficients,m_NumberCoefficients));
  LaplacianBaltrami->fill(0.0);
  MatrixDoublePtr FRTMatrix(new vnl_matrix<double>(m_NumberCoefficients,m_NumberCoefficients));
  FRTMatrix->fill(0.0);
  MatrixDoublePtr SHEigenvalues(new vnl_matrix<double>(m_NumberCoefficients,m_NumberCoefficients));
  SHEigenvalues->fill(0.0);



  // SHBasis-Matrix + LaplacianBaltrami-Matrix + SHOrderAssociationVector
  ComputeSphericalHarmonicsBasis(Q.get() ,m_SHBasisMatrix, LaplacianBaltrami.get(), SHOrderAssociation.get(), SHEigenvalues.get());

  // Compute FunkRadon Transformation Matrix Associated to SHBasis Order lj
  ComputeFunkRadonTransformationMatrix(SHOrderAssociation.get() ,FRTMatrix.get());

  MatrixDoublePtr temp(new vnl_matrix<double>(((m_SHBasisMatrix->transpose()) * (*m_SHBasisMatrix)) + (m_Lambda  * (*LaplacianBaltrami))));

  InverseMatrixDoublePtr pseudo_inv(new vnl_matrix_inverse<double>((*temp)));
  MatrixDoublePtr inverse(new vnl_matrix<double>(m_NumberCoefficients,m_NumberCoefficients));
  inverse->fill(0.0);
  (*inverse) = pseudo_inv->inverse();

  // ODF Factor ( missing 1/4PI ?? )
  double factor = (1.0/(16.0*QBALL_ANAL_RECON_PI*QBALL_ANAL_RECON_PI));

  m_SignalReonstructionMatrix = new vnl_matrix<double>((*inverse) * (m_SHBasisMatrix->transpose()));

  m_CoeffReconstructionMatrix = new vnl_matrix<double>(( factor * ((*FRTMatrix) * ((*SHEigenvalues) * (*m_SignalReonstructionMatrix))) ));

  // this code goes to the image adapter coeffs->odfs later

  vnl_matrix_fixed<double, 3, NOdfDirections>* U = itk::PointShell<NOdfDirections, vnl_matrix_fixed<double, 3, NOdfDirections> >::DistributePointShell();

  m_ODFSphericalHarmonicBasisMatrix  = new vnl_matrix<double>(NOdfDirections,m_NumberCoefficients);
  m_ODFSphericalHarmonicBasisMatrix->fill(0.0);

  for(int i=0; i<NOdfDirections; i++)
  {
    double x = (*U)(0,i);
    double y = (*U)(1,i);
    double z = (*U)(2,i);
    double cart[3];
    mitk::ShericalHarmonicsFunctions::Cart2Sph(x,y,z,cart);
    (*U)(0,i) = cart[0];
    (*U)(1,i) = cart[1];
    (*U)(2,i) = cart[2];
  }

  for(int i=0; i<NOdfDirections; i++)
  {
    for(int k=0; k<=LOrder; k+=2)
    {
      for(int m=-k; m<=k; m++)
      {
        int j = (k*k + k + 2)/2 + m - 1;
        double phi = (*U)(0,i);
        double th = (*U)(1,i);
        (*m_ODFSphericalHarmonicBasisMatrix)(i,j) = mitk::ShericalHarmonicsFunctions::Yj(m,k,th,phi);
      }
    }
  }

  m_ReconstructionMatrix = new vnl_matrix<double>((*m_ODFSphericalHarmonicBasisMatrix) * (*m_CoeffReconstructionMatrix));

}

template< class T, class TG, class TO, int L, int NODF>
template< class VNLType >
void DiffusionMultiShellQballReconstructionImageFilter<T,TG,TO,L,NODF>
::printMatrix( VNLType * mat  )
{

  std::stringstream stream;

  for(int i = 0 ; i < mat->rows(); i++)
  {
    stream.str("");
    for(int j = 0; j < mat->cols(); j++)
    {
      stream << (*mat)(i,j) << "  ";
    }

  }


  MITK_INFO << stream.str();

}

template< class T, class TG, class TO, int L, int NODF>
bool DiffusionMultiShellQballReconstructionImageFilter<T,TG,TO,L,NODF>
::CheckDuplicateDiffusionGradients()
{

  GradientDirectionContainerType::ConstIterator gdcit1;
  GradientDirectionContainerType::ConstIterator gdcit2;

  for(gdcit1 = this->m_GradientDirectionContainer->Begin(); gdcit1 != this->m_GradientDirectionContainer->End(); ++gdcit1)
  {
    for(gdcit2 = this->m_GradientDirectionContainer->Begin(); gdcit2 != this->m_GradientDirectionContainer->End(); ++gdcit2)
    {
      if(gdcit1.Value() == gdcit2.Value() && gdcit1.Index() != gdcit2.Index())
      {
        itkWarningMacro( << "Some of the Diffusion Gradients equal each other. Corresponding image data should be averaged before calling this filter." );
        return true;
      }
    }
  }
  return false;
}


template< class T, class TG, class TO, int L, int NODF>
void DiffusionMultiShellQballReconstructionImageFilter<T,TG,TO,L,NODF>
::PrintSelf(std::ostream& os, Indent indent) const
{
  std::locale C("C");
  std::locale originalLocale = os.getloc();
  os.imbue(C);

  Superclass::PrintSelf(os,indent);

  os << indent << "OdfReconstructionMatrix: " << m_ReconstructionMatrix << std::endl;
  if ( m_GradientDirectionContainer )
  {
    os << indent << "GradientDirectionContainer: "
       << m_GradientDirectionContainer << std::endl;
  }
  else
  {
    os << indent <<
          "GradientDirectionContainer: (Gradient directions not set)" << std::endl;
  }
  os << indent << "NumberOfGradientDirections: " <<
        m_NumberOfGradientDirections << std::endl;
  os << indent << "NumberOfBaselineImages: " <<
        m_NumberOfBaselineImages << std::endl;
  os << indent << "Threshold for reference B0 image: " << m_Threshold << std::endl;
  os << indent << "BValue: " << m_BValue << std::endl;

  os.imbue( originalLocale );
}




}

#endif // __itkDiffusionMultiShellQballReconstructionImageFilter_cpp
