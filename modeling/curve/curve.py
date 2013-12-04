#save curves shapes for the controls of the rig
import general.mayaNode.mayaNode as mn
reload( mn )
import maya.cmds as mc
import json

def correctNameForCtlCurves():
	#rename ctl
	for n in mn.ls( '*_ctl' ):
		child = mn.listRelatives( n.name, s = True, ni = True, f = True )
		print child
		if child:
			print n.name, child[0].name, n.name + 'Shape'
			child[0].name = n.name + 'Shape'
		print n.name,child

def exportRigCurvesShapes( curvesDataPath = '' ):
	ignoredCurves = [ 
					  "hand_pinky_ctl",
					  "hand_ring_ctl",
					  "hand_middle_ctl",
					  "hand_pointer_ctl",
					  "hand_thumb_ctl",
					  "fingers"
					]
	correctNameForCtlCurves()	
	crvs = [ a for a in mn.ls( '*_ctl*', typ = 'nurbsCurve' ) if not any( ig in a.name for ig in ignoredCurves ) ]
	crvsData = {}
	for c in crvs:
		crvsData[ c.name ] = Curve( c ).cvsPosition
	with open(curvesDataPath, 'w') as outfile:
		json.dump(crvsData, outfile)

def importRigCurvesShapes( curvesDataPath = ''):
	"""read from json file the position of the verteces of the curves for the rig"""
	data = []
	with open( curvesDataPath ) as f:
		data = json.load(f)
	correctNameForCtlCurves()	
	for c in data.keys():
		print c
		Curve( c ).cvsPosition = data[ c ]

def copyVertexPositionForSelectedCurves( searchAndRelapce = ['l_', 'r_'], mirrorAxis = 'x', worldSpace = True ):
	"""copy vertex position on the selected curves"""
	sels = mn.ls( sl = True, dag = True, typ = 'nurbsCurve', ni = True )
	for s in sels:
		copyVertexPositions( s, s.name.replace( searchAndRelapce[0], searchAndRelapce[1], 1 ), mirrorAxis, worldSpace )

def copyVertexPositions( curveToCopy, curveToPaste, mirrorAxis = 'None', worldSpace = True ):
	"""copy verteces positions from one curve to another"""
	origCurve = Curve( curveToCopy )
	finalCurve = Curve( curveToPaste )
	if origCurve.cvsCount != finalCurve.cvsCount:
		raise "The number of verteces of the curves are different", curveToCopy, curveToPaste
	AxisToMirror = {'None': [1,1,1],
					'x' : [-1,1,1],
					'y' : [1,-1,1],
					'z' : [1,1,-1],
					}
	origCurveCvsPos = origCurve.cvsPosition
	finalAxis = AxisToMirror[ mirrorAxis ]
	finalCurve.cvsPosition = [ [p[0] * finalAxis[0], p[1]*finalAxis[1], p[2]*finalAxis[2]] for p in origCurveCvsPos ]

class Curve(mn.Node):
	"""curve object"""
	def __init__(self, name):
		super( Curve, self ).__init__( name )

	@property
	def degree(self):
		"""return the degree of the curve"""
		return self.curve.a.degree.v

	@property
	def spans(self):
		"""return the spans of the curve"""
		return self.curve.a.spans.v

	@property
	def cvsCount(self):
		"""return the number of cvs of the curve"""
		cvsCount = self.degree + self.spans
		return cvsCount

	@property
	def transform(self):
		"""return the transform node of the curve"""
		return self.curve.parent

	@property
	def cvsPosition(self):
		"""return an array with the position of the verteces in worldspace"""
		cvsCount = self.cvsCount
		cvsPos = []
		for cv in range( cvsCount ):
			vPos = self.getVertexPosition( cv, True ) 
			cvsPos.append( vPos )
		return cvsPos

	@cvsPosition.setter
	def cvsPosition(self, newPositions):
		"""set the new position for the verteces"""
		cvsCount = self.cvsCount
		if len( newPositions ) != cvsCount:
			raise "The number of new positions is not the same as the number of vertices of the curve"
		for cv in range( cvsCount ):
			self.setVertexPosition( cv, newPositions[cv], True )

	def getVertexPosition(self, vertexNumber, worldSpace = True):
		"""return the position of a vertex in world space"""
		vPos = mc.xform( self.curve.name + '.cv[%i'%vertexNumber + ']', q = True, ws = worldSpace, t = True )
		return vPos

	def setVertexPosition(self, vertexNumber, newPos, worldSpace = True):
		"""docstring for setVertexPosition"""
		mc.xform( self.curve.name + '.cv[%i'%vertexNumber + ']', ws = worldSpace, t = newPos )

	def create(self, shape):
		"""create a curve with a specific shape"""
		if shape == "arrow":
			mc.curve( d =1, p = [( 0, 0.6724194, 0.4034517 ),( 0, 0, 0.4034517 ),( 0, 0, 0.6724194 ),( 0, -0.4034517, 0 ),( 0, 0, -0.6724194 ),( 0, 0, -0.4034517 ),( 0, 0.6724194, -0.4034517 ),( 0, 0.6724194, 0.4034517 )] ,k=[ 0 , 1 , 2 , 3 , 4 , 5 , 6 , 7 ],n = self.name )
		elif shape == "cross":
			mc.curve( d =1, p = [( 1, 0, -1 ),( 2, 0, -1 ),( 2, 0, 1 ),( 1, 0, 1 ),( 1, 0, 2 ),( -1, 0, 2 ),( -1, 0, 1 ),( -2, 0, 1 ),( -2, 0, -1 ),( -1, 0, -1 ),( -1, 0, -2 ),( 1, 0, -2 ),( 1, 0, -1 )] , k=[ 0 , 1 , 2 , 3 , 4 , 5 , 6 , 7 , 8 , 9 , 10 , 11 , 12 ] ,n = self.name )
		elif shape == "square":
			mc.curve( d =1, p = [( -1, 0, 1 ),( 1, 0, 1 ),( 1, 0, -1 ),( -1, 0, -1 ),( -1, 0, 1 )] ,k = [ 0 , 1 , 2 , 3 , 4 ],n = self.name )
		
		elif shape == "cube":
			mc.curve( d =1, p = [( -0.5, 0.5, 0.5 ),( 0.5, 0.5, 0.5 ),( 0.5, 0.5, -0.5 ),( -0.5, 0.5, -0.5 ),( -0.5, 0.5, 0.5 ),( -0.5, -0.5, 0.5 ),( -0.5, -0.5, -0.5 ),( 0.5, -0.5, -0.5 ),( 0.5, -0.5, 0.5 ),( -0.5, -0.5, 0.5 ),( 0.5, -0.5, 0.5 ),( 0.5, 0.5, 0.5 ),( 0.5, 0.5, -0.5 ),( 0.5, -0.5, -0.5 ),( -0.5, -0.5, -0.5 ),( -0.5, 0.5, -0.5 )] , k = [ 0 , 1 , 2 , 3 , 4 , 5 , 6 , 7 , 8 , 9 , 10 , 11 , 12 , 13 , 14 , 15 ] ,n = self.name)
		elif shape == "orient":
			mc.curve( d =3, p = [( 0.0959835, 0.604001, -0.0987656 ),( 0.500783, 0.500458, -0.0987656 ),( 0.751175, 0.327886, -0.0987656 ),( 0.751175, 0.327886, -0.0987656 ),( 0.751175, 0.327886, -0.336638 ),( 0.751175, 0.327886, -0.336638 ),( 1.001567, 0, 0 ),( 1.001567, 0, 0 ),( 0.751175, 0.327886, 0.336638 ),( 0.751175, 0.327886, 0.336638 ),( 0.751175, 0.327886, 0.0987656 ),( 0.751175, 0.327886, 0.0987656 ),( 0.500783, 0.500458, 0.0987656 ),( 0.0959835, 0.604001, 0.0987656 ),( 0.0959835, 0.604001, 0.0987656 ),( 0.0959835, 0.500458, 0.500783 ),( 0.0959835, 0.327886, 0.751175 ),( 0.0959835, 0.327886, 0.751175 ),( 0.336638, 0.327886, 0.751175 ),( 0.336638, 0.327886, 0.751175 ),( 0, 0, 1.001567 ),( 0, 0, 1.001567 ),( -0.336638, 0.327886, 0.751175 ),( -0.336638, 0.327886, 0.751175 ),( -0.0959835, 0.327886, 0.751175 ),( -0.0959835, 0.327886, 0.751175 ),( -0.0959835, 0.500458, 0.500783 ),( -0.0959835, 0.604001, 0.0987656 ),( -0.0959835, 0.604001, 0.0987656 ),( -0.500783, 0.500458, 0.0987656 ),( -0.751175, 0.327886, 0.0987656 ),( -0.751175, 0.327886, 0.0987656 ),( -0.751175, 0.327886, 0.336638 ),( -0.751175, 0.327886, 0.336638 ),( -1.001567, 0, 0 ),( -1.001567, 0, 0 ),( -0.751175, 0.327886, -0.336638 ),( -0.751175, 0.327886, -0.336638 ),( -0.751175, 0.327886, -0.0987656 ),( -0.751175, 0.327886, -0.0987656 ),( -0.500783, 0.500458, -0.0987656 ),( -0.0959835, 0.604001, -0.0987656 ),( -0.0959835, 0.604001, -0.0987656 ),( -0.0959835, 0.500458, -0.500783 ),( -0.0959835, 0.327886, -0.751175 ),( -0.0959835, 0.327886, -0.751175 ),( -0.336638, 0.327886, -0.751175 ),( -0.336638, 0.327886, -0.751175 ),( 0, 0, -1.001567 ),( 0, 0, -1.001567 ),( 0.336638, 0.327886, -0.751175 ),( 0.336638, 0.327886, -0.751175 ),( 0.0959835, 0.327886, -0.751175 ),( 0.0959835, 0.327886, -0.751175 ),( 0.0959835, 0.500458, -0.500783 ),( 0.0959835, 0.604001, -0.0987656 )] , k = [ 0 , 0 , 0 , 1 , 2 , 3 , 4 , 5 , 6 , 7 , 8 , 9 , 10 , 11 , 12 , 13 , 14 , 15 , 16 , 17 , 18 , 19 , 20 , 21 , 22 , 23 , 24 , 25 , 26 , 27 , 28 , 29 , 30 , 31 , 32 , 33 , 34 , 35 , 36 , 37 , 38 , 39 , 40 , 41 , 42 , 43 , 44 , 45 , 46 , 47 , 48 , 49 , 50 , 51 , 52 , 53 , 53 , 53 ],n = self.name )
		elif shape == "circleY":
			mc.circle( c = ( 0, 0, 0 ) ,nr = ( 0, 1, 0 ), sw= 360, r= 1, d =3, ut= 0, tol= 0.01, s= 8, ch= 1, n =  self.name )
		elif shape == "circleZ":
			mc.circle( c = ( 0, 0, 0 ) ,nr = ( 0, 0, 1 ), sw= 360, r= 1, d =3, ut= 0, tol= 0.01, s= 8, ch= 1, n = self.name )
		elif shape == "circleX":
			mc.circle( c = ( 0, 0, 0 ) ,nr = ( 1, 0, 0 ), sw= 360, r= 1, d =3, ut= 0, tol= 0.01, s= 8, ch= 1, n = self.name )
		elif shape == "sphere":
			mc.curve( d = 1 , p =[ ( 0, 3, 0 ),( 0, 2, -2 ),( 0, 0, -3 ),( 0, -2, -2 ),( 0, -3, 0 ),( 0, -2, 2 ),( 0, 0, 3 ),( 0, 2, 2 ),( 0, 3, 0 ),( 2, 2, 0 ),( 3, 0, 0 ),( 2, -2, 0 ),( 0, -3, 0 ),( -2, -2, 0 ),( -3, 0, 0 ),( -2, 2, 0 ),( 0, 3, 0 )], k = [ 0 , 1 , 2 , 3 , 4 , 5 , 6 , 7 , 8 , 9 , 10 , 11 , 12 , 13 , 14 , 15 , 16 ] ,n = self.name )
		elif shape == "plus":
			mc.curve( d = 1 , p = [( 0, 1, 0 ),( 0, -1, 0 ),( 0, 0, 0 ),( -1, 0, 0 ),( 1, 0, 0 ),( 0, 0, 0 ),( 0, 0, 1 ),( 0, 0, -1 )], k = [ 0 , 1 , 2 , 3 , 4 , 5 , 6 , 7 ] ,n = self.name )
		else:
			print "THERE IS NO CURVE FOR THE SHAPE THAT YOU WANT!"


