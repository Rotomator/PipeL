import _ExocortexAlembicPython as alembic
import sys
import argparse

# ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
# global variables
b_verbose = False
obj_filter = None
typ_filter = None
noo_filter = None
not_filter = None

# ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
#copy directly a property and its corresponding values
def copy_property(prop, outProp, full_name):
   if b_verbose:
      print("PROP: \"" + full_name + "/" + prop.getName() + "\" of type " + prop.getType() + " with " + str(prop.getNbStoredSamples()) + " stored samples")
   for i in xrange(0, prop.getNbStoredSamples()):
      vals = prop.getValues(i)
      outProp.setValues(vals)

# ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
#visiting the structure, if it's a property, copy it, if it's a compound, continue the visit there
def copy_compound_property(cprop, outCprop, full_name):
   full_name = full_name + "/" + cprop.getName()
   if b_verbose:
      print("COMP: \"" + full_name + "\"")
   for prop_name in cprop.getPropertyNames():
      if prop_name == ".metadata":
         continue                                                    # .metadata cause some problem

      if prop.isCompound():
         out_prop = outCprop.getProperty(prop_name, prop.getType())
         copy_compound_property(prop, out_prop, out_data)
      else:
         curTS = prop.getSampleTimes()

         out_prop = None
         if len(curTS.getTimeSamples()) == 0:
            out_prop = outCprop.getProperty(prop_name, prop.getType())
         else:
            tsSampling = out_data.createTimeSampling([curTS])
            out_prop = outCprop.getProperty(prop_name, prop.getType(), tsSampling[0])

         copy_property(prop, out_prop)

# ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
# going through each object
def copy_objects(in_data, out_data):
   global obj_filter
   global typ_filter
   global noo_filter
   global not_filter
   
   for identifier in in_data.getIdentifiers():
      if (obj_filter != None and identifier.find(obj_filter) < 0) or (noo_filter != None and identifier.find(noo_filter) >= 0):
         continue                                                    # pass over this object!
      
      obj = in_data.getObject(identifier)
      obj_typ = obj.getType()
      if (typ_filter != None and obj_typ.find(typ_filter) < 0) or (not_filter != None and obj_typ.find(not_filter) >= 0):
         continue                                                    # pass over this object because of its type!
      
      if b_verbose:
         print("OBJ : \"" + identifier + "\" of type " + obj_typ)
      
      curTS = obj.getSampleTimes() 
      out = None
      if len(curTS.getTimeSamples()) == 0:
         out = out_data.createObject(obj_typ, identifier)
      else:
         tsSampling = out_data.createTimeSampling([curTS])
         out = out_data.createObject(obj_typ, identifier, tsSampling[0])

      out.setMetaData(obj.getMetaData())
      for prop_name in obj.getPropertyNames():
         if prop_name == ".metadata":
            continue                                                 # .metadata cause some problem
         prop = obj.getProperty(prop_name)

         if prop.isCompound():
            out_prop = out.getProperty(prop_name, prop.getType())
            copy_compound_property(prop, out_prop, out_data)
         else:
            curTS = prop.getSampleTimes()

            out_prop = None
            if len(curTS.getTimeSamples()) == 0:
               out_prop = out.getProperty(prop_name, prop.getType())
            else:
               tsSampling = out_data.createTimeSampling([curTS])
               out_prop = out.getProperty(prop_name, prop.getType(), tsSampling[0])

            copy_property(prop, out_prop)

# ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
def main(args):
   global b_verbose
   global obj_filter
   global typ_filter
   global noo_filter
   global not_filter
   
   # parser args
   parser = argparse.ArgumentParser(description="Copy integrally, or partially, an alembic file into a second alembic file")
   parser.add_argument("abc_in", type=str, metavar="{Alembic input file}", help="input alembic file to be copied")
   parser.add_argument("-o", type=str, metavar="{Alembic output file}", help="optional output file name, default is \"a.abc\"", default="a.abc")
   parser.add_argument("-v", "--verbose", action='store_true', help='show the details of the copy')
   parser.add_argument("-f", "--filter", type=str, metavar="{id filter}", help="only copy objects containing substring {id filter} in their identifier")
   parser.add_argument("-T", "--typefilter", type=str, metavar="{type filter}", help="only copy objects containing substring {type filter} in their type")
   parser.add_argument("-nf", "--NOTfilter", type=str, metavar="{id filter}", help="only copy objects NOT containing substring {id filter} in their identifier")
   parser.add_argument("-nT", "--NOTtypefilter", type=str, metavar="{type filter}", help="only copy objects NOT containing substring {type filter} in their type")
   ns = vars(parser.parse_args(args[1:]))
   
   if ns["abc_in"] == ns["o"]:
      print("Error: input and output filenames must be different")
      return
   
   b_verbose = ns["verbose"]
   obj_filter = ns["filter"]
   typ_filter = ns["typefilter"]
   noo_filter = ns["NOTfilter"]
   not_filter = ns["NOTtypefilter"]
   
   in_data = alembic.getIArchive(ns["abc_in"])
   out_data = alembic.getOArchive(ns["o"])
   out_data.createTimeSampling(in_data.getSampleTimes())
   copy_objects(in_data, out_data)

# ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
if __name__ == "__main__":
   main(sys.argv)


