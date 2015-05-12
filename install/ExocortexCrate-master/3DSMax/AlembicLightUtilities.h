#ifndef _ALEMBIC_LIGHT_UTILITIES_H_
#define _ALEMBIC_LIGHT_UTILITIES_H_

#include <string>


int AlembicImport_Light(const std::string &path, AbcG::IObject& iObj, alembic_importoptions &options, INode** pMaxNode);

AbcM::IMaterialSchema getMatSchema(AbcG::ILight& objLight);
//Abc::IFloatProperty readShaderScalerProp(AbcM::IMaterialSchema& matSchema, const std::string& shaderTarget, const std::string& shaderType, const std::string& shaderProp);


//Abc::IFloatProperty
template<class PT> PT readShaderScalerProp(AbcM::IMaterialSchema& matSchema, const std::string& shaderTarget, const std::string& shaderType, const std::string& shaderProp)
{
   Abc::ICompoundProperty propk = matSchema.getShaderParameters(shaderTarget, shaderType);

   for(size_t i=0; i<propk.getNumProperties(); i++){
      AbcA::PropertyHeader pheader = propk.getPropertyHeader(i);
      AbcA::PropertyType propType = pheader.getPropertyType();

      if( propType == AbcA::kScalarProperty ){

         if(boost::iequals(pheader.getName(), shaderProp) && PT::matches(pheader))
         {
            return PT(propk, pheader.getName());
         }

      }

   }
   
   ESS_LOG_WARNING("Could not read shader property: "<<shaderTarget<<"."<<shaderType<<"."<<shaderProp);
   
   return PT();
}

#endif