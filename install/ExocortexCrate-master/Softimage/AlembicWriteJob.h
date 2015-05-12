#ifndef _ALEMBIC_WRITE_JOB_H_
#define _ALEMBIC_WRITE_JOB_H_

#include "AlembicObject.h"
#include "CommonSceneGraph.h"

class AlembicWriteJob
{
private:
    XSI::CString mFileName;
   
    std::vector<std::string> mSelection;

    //XSI::CRefArray mSelection;
    std::vector<double> mFrames;
    Abc::OArchive mArchive;
	Abc::OObject mTop;
    unsigned int mTs;
    
    std::vector<AlembicObjectPtr> mObjects;
	std::map<std::string,AlembicObjectPtr> mObjectsNames;
    float mFrameRate;

   SceneNodePtr exoSceneRoot;
public:

   std::map<XSI::CString,XSI::CValue> mOptions;

   AlembicWriteJob(
      const XSI::CString & in_FileName,
      const std::vector<std::string>& in_Selection,
      const XSI::CDoubleArray & in_Frames);
   ~AlembicWriteJob();

   Abc::OArchive GetArchive() { return mArchive; }
   Abc::OObject GetTop() { return mTop; }
   const std::vector<double> & GetFrames() { return mFrames; }
   const XSI::CString & GetFileName() { return mFileName; }
   unsigned int GetAnimatedTs() { return mTs; }
   void SetOption(const XSI::CString & in_Name, const XSI::CValue & in_Value);
   bool HasOption(const XSI::CString & in_Name);
   XSI::CValue GetOption(const XSI::CString & in_Name);
   //AlembicObjectPtr GetObject(const XSI::CRef & in_Ref);
   bool AddObject(AlembicObjectPtr in_Obj);
   size_t GetNbObjects() { return mObjects.size(); }
 
   XSI::CStatus PreProcess();
   XSI::CStatus Process(double frame);
};

#endif