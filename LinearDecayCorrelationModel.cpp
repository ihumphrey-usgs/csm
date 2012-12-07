//#############################################################################
//
//    FILENAME:          LinearDecayCorrelationModel.cpp
//
//    CLASSIFICATION:    Unclassified
//
//    DESCRIPTION:
//
//    This class is used to compute the correlation between adjustable
//    parameters in a community sensor model (CSM).
//
//    The class holds correlation data in the form of two lists. The first is a
//    list of times that specify the start times of each time segment.
//    The second list spcifies the correlation at the specified time. Between
//    specified times the correlation is a linear imterpolation in time of the 
//    correlation at each end of the time segment.
//
//    The class assumes that sensor model parameters are divided into disjoint
//    groups, such that the correlation coefficient between any two parameters
//    in the same group is given by the equation above, and the correlation
//    between two parameters in different groups is 0.0.  This implementation
//    provides a way to assign sensor model parameters to groups, to set the
//    values of the correlation parameters for a given group, and to compute
//    the equation above.
//
//    LIMITATIONS:       None
//
//
//    SOFTWARE HISTORY:
//     Date          Author   Comment
//     -----------   ------   -------
//     20-Nov-2012   ISK      Initial Release.
//     30-Oct-2012   SCM      Renamed to LinearDecayCorrelationModel.h
//
//    NOTES:
//     Refer to LinearDecayCorrelationModel.h for more information.
//#############################################################################
#define CSM_LIBRARY
#include "LinearDecayCorrelationModel.h"
#include "Error.h"

#ifdef IRIXN32
#include "math.h"
#else
#include <cmath>
using std::exp;
using std::fabs;
#endif

namespace csm {

LinearDecayCorrelationModel::LinearDecayCorrelationModel(size_t numSMParams,
                                                         size_t numCPGroups)
   :
      theGroupMapping(numSMParams, -1),
      theCorrParams(numSMParams)
{
   // set the format string in the base class
   theFormat = "LinearDecayCorrelation";
}

LinearDecayCorrelationModel::~LinearDecayCorrelationModel()
{
}

size_t LinearDecayCorrelationModel::getNumSensorModelParameters() const
{
   return theGroupMapping.size();
}

size_t LinearDecayCorrelationModel::getNumCorrelationParameterGroups() const
{
   return theCorrParams.size();
}

int LinearDecayCorrelationModel::getCorrelationParameterGroup(size_t smParamIndex) const
{
   // make sure the index falls within the acceptable range
   checkSensorModelParameterIndex(smParamIndex, "getCorrelationParameterGroup");

   // return the correlation parameter group index by reference
   return theGroupMapping[smParamIndex];
}

void LinearDecayCorrelationModel::setCorrelationParameterGroup(
   size_t smParamIndex,
   size_t cpGroupIndex)
{
   // make sure the indices fall within the acceptable ranges
   checkSensorModelParameterIndex(smParamIndex, "setCorrelationParameterGroup");
   checkParameterGroupIndex(cpGroupIndex, "setCorrelationParameterGroup");

   // set the group index for the given sensor model parameter
   theGroupMapping[smParamIndex] = cpGroupIndex;
}

void LinearDecayCorrelationModel::setCorrelationGroupParameters(
   size_t cpGroupIndex,
   const std::vector<double>& initialCorrsPerSegment,
   const std::vector<double>& timesPerSegment)
{
   setCorrelationGroupParameters(
      cpGroupIndex, Parameters(initialCorrsPerSegment, timesPerSegment) );
}

void LinearDecayCorrelationModel::setCorrelationGroupParameters(
   size_t            cpGroupIndex,
   const Parameters& params)
{
   static const char* const MODULE =
      "csm::LinearDecayCorrelationModel::setCorrelationGroupParameters";

   // make sure the index falls within the acceptable range
   checkParameterGroupIndex(cpGroupIndex, "setCorrelationGroupParameters");

   // make sure the values of each correlation model parameter
   // fall within acceptable ranges
   size_t size = params.theInitialCorrsPerSegment.size();
   if (size != params.theTimesPerSegment.size())
   {
      throw Error(
         Error::BOUNDS,
         "Must have equal number of correlations and times.",
         MODULE);
   }

   double corr, prevCorr;
   double time, prevTime;

   if (size > 1)
   {
      for(size_t i = 0; i < size; ++i)
      {
         corr = params.theInitialCorrsPerSegment[i];
         time = params.theTimesPerSegment[i];
         if (corr < 0.0 || corr > 1.0)
         {
            throw Error(
               Error::BOUNDS,
               "Correlation must be in range [0..1].",
               MODULE);
         }

         if (i > 0)
         {
            prevCorr = params.theInitialCorrsPerSegment[i-1];
            prevTime = params.theTimesPerSegment[i-1];
            if (corr > prevCorr)
            {
               throw Error(
                  Error::BOUNDS,
                  "Correlation must be monotomically decreasing.",
                  MODULE);
            }
            if (time < prevTime)
            {
               throw Error(
                  Error::BOUNDS,
                  "Time must be monotomically increasing.",
                  MODULE);
            }
         }
      }
   }

   // store the correlation parameter values
   theCorrParams[cpGroupIndex] = params;
}

double LinearDecayCorrelationModel::getCorrelationCoefficient(
   size_t cpGroupIndex,
   double deltaTime) const
{
   // make sure the index falls within the acceptable range
   checkParameterGroupIndex(cpGroupIndex, "getCorrelationCoefficient");

   // compute the value of the correlation coefficient
   const Parameters& cp = theCorrParams[cpGroupIndex];
   const size_t size = cp.theInitialCorrsPerSegment.size();

   const double adt = fabs(deltaTime);
   double prevCorr = cp.theInitialCorrsPerSegment[0];
   double prevTime = cp.theTimesPerSegment[0];

   double correlation = prevCorr;

   for(size_t s = 1; s < size; ++s)
   {
      const double corr = cp.theInitialCorrsPerSegment[s];
      const double time = cp.theTimesPerSegment[s];
      if (adt <= time)
      {
         if (time - prevTime != 0.0)
         {
            correlation =
               prevCorr +
               (adt - prevTime) / (time - prevTime) * (corr - prevCorr);
         }
         break;
      }
      prevCorr = corr;
      prevTime = time;
      correlation = prevCorr;
   }

   // if necessary, clamp the coefficient value to the acceptable range
   if (correlation < 0.0)
   {
      correlation =  0.0;
   }
   else if (correlation > 1.0)
   {
      correlation = 1.0;
   }
   
   return correlation;
}

const LinearDecayCorrelationModel::Parameters&
LinearDecayCorrelationModel::getCorrelationGroupParameters(
   size_t cpGroupIndex) const
{
   // make sure the index falls within the acceptable range
   checkParameterGroupIndex(cpGroupIndex, "getCorrelationGroupParameters");

   return theCorrParams[cpGroupIndex];
}

void LinearDecayCorrelationModel::checkSensorModelParameterIndex(
   size_t             smParamIndex, 
   const std::string& functionName) const
{
   if (smParamIndex >= theGroupMapping.size())
   {
      throw Error(
         Error::INDEX_OUT_OF_RANGE,
         "Sensor model parameter index is out of range.",
         "csm::LinearDecayCorrelationModel::" + functionName);
   }
}

void LinearDecayCorrelationModel::checkParameterGroupIndex(
   size_t             groupIndex,
   const std::string& functionName) const
{
   if (groupIndex >= theCorrParams.size())
   {
      throw Error(
         Error::INDEX_OUT_OF_RANGE,
         "Correlation parameter group index is out of range.",
         "csm::LinearDecayCorrelationModel::" + functionName);
   }
}

} // namespace csm