clearlistener()
global modelScale = 1
global debugMode = true

function WriteLog txt = 
(
	if debugMode then print (" " + txt)
)

function ReadFixedString stream len =
(
	local str = ""
	for x = 1 to len do
	(
		cbyte = readbyte stream
		if cbyte != 0x00 then str += bit.intaschar(cbyte)
	)	
	str
)

function ReadCharss stream len =
(
	local str = ""
	for x = 1 to len do
	(
		cbyte = readbyte stream
		if cbyte == 0x00 then  (str += "|")
		else  (str += bit.intaschar(cbyte))
	)	
	str
)

-- file settings
-- filepath = "E:\DxgReader\GM4_01_006.dxg"   -- 0x350
filepath = "E:\DxgReader\GM4_03_206.dxg"    --  0x277fe
-- filepath = "E:\DxgReader\GM4_02_002.dxg"    --  0x109fd
boneSeekPos = 0x277fe

WriteLog("File: " + filepath as string)
-- binary reader (open file)
f = fopen filepath "rb"
CurrentDir = getFilenamePath filepath

-- file header
Magic = readlong f
WriteLog("Magic: " + Magic as string)

if Magic == 0x20475844 then
(
	Version = readlong f
	WriteLog("Version: " + Version as string)
	
	flag = readlong f #unsigned -- 7
	groupCount = readlong f #unsigned
	next_data_offset = readlong f #unsigned
	WriteLog("groupCount Count: " + groupCount as string)

	name_size = readlong f #unsigned
	nameBuf = ReadCharss f name_size
	material_names = filterString (nameBuf as string) "|"
	WriteLog("Material Names Count: " + material_names.count as string)
	for i = 1 to material_names.count do WriteLog(material_names[i] as string)

	fseek f (boneSeekPos) #seek_set
	
	BonesCount = readlong f
	WriteLog("Bone Count: " + BonesCount as string)
	bone_data_size = readlong f
	bone_names_size = readlong f
	bone_nameBuf = ReadCharss f bone_names_size
	bone_names = filterString bone_nameBuf "|"
	-- WriteLog(bone_nameBuf)
	WriteLog("Bone Names Count: " + bone_names.count as string)
	for i = 1 to bone_names.count do WriteLog(bone_names[i] as string)

	BonesArr = #()
	for i = 1 to BonesCount do  --读父子关系
	( 
		local cBone
		index =  readbyte f
		parent =  readbyte f
		child =  readbyte f
		sibling =  readbyte f

		cBone = bonesys.createbone [0,0,0] [0,0,0] [0,0,0]
		cBone.showlinks = true
		--cBone.showlinksonly = true
		cBone.wirecolor = yellow
		cBone.width = 0.3
		cBone.height = 0.3

		cBone.pos.controller = TCB_position ()
		cBone.rotation.controller = TCB_rotation ()

		cBone.name = bone_names[i]
		-- cBone.transform = tfm
		if parent > -1 then 
		(
			cBone.parent = BonesArr[parent + 1]
			-- cBone.transform *= cBone.parent.transform
		) 
		append BonesArr cBone
	)

	for i = 1 to BonesCount do  --读矩阵
	(
		-- Bone Matrix Data
		m11 = readfloat f; m12 = readfloat f; m13 = readfloat f; m14 = readfloat f;
		m21 = readfloat f; m22 = readfloat f; m23 = readfloat f; m24 = readfloat f; 
		m31 = readfloat f; m32 = readfloat f; m33 = readfloat f; m34 = readfloat f; 
		m41 = readfloat f; m42 = readfloat f; m43 = readfloat f; m44 = readfloat f; 

		tfm = (matrix3 [m11, m12, m13] [m21, m22, m23] [m31, m32, m33] ([m41, m42, m43] * m44 * modelScale) )

		tfm = inverse tfm   -- MaxScript 坑爹，一定要反转矩阵才对

		cBone = BonesArr[i]
		cBone.transform = tfm
	)
	print ("Finish at position: " + (bit.intAsHex (ftell f)))
)
else(
	print "Invalid DXG file format..."
)

