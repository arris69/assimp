/*
 ---------------------------------------------------------------------------
 Open Asset Import Library (assimp)
 ---------------------------------------------------------------------------

 Copyright (c) 2006-2014, assimp team

 All rights reserved.

 Redistribution and use of this software in source and binary forms,
 with or without modification, are permitted provided that the following
 conditions are met:

 * Redistributions of source code must retain the above
 copyright notice, this list of conditions and the
 following disclaimer.

 * Redistributions in binary form must reproduce the above
 copyright notice, this list of conditions and the
 following disclaimer in the documentation and/or other
 materials provided with the distribution.

 * Neither the name of the assimp team, nor the names of its
 contributors may be used to endorse or promote products
 derived from this software without specific prior
 written permission of the assimp team.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ---------------------------------------------------------------------------
 */

/** @file  GEOLoader.cpp
 *  @brief Implementation of the GEO importer class
 */

#include "AssimpPCH.h"
#ifndef ASSIMP_BUILD_NO_GEO_IMPORTER

// internal headers
#include "GEOLoader.h"
#include "ParsingUtils.h"
#include "fast_atof.h"

#include "TinyFormatter.h"

#include "GEOColorTable.h"

using namespace Assimp;

// helper macro to determine the size of an array
#if (!defined ARRAYSIZE)
#	define ARRAYSIZE(_array) (int(sizeof(_array) / sizeof(_array[0])))
#endif

#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER) /* TODO: more sense here... */
//#	define snprintf _snprintf
//#	define vsnprintf _vsnprintf
//#	define strcasecmp _stricmp
//#	define strncasecmp _strnicmp
/*
 Windows: stricmp()
 warning: implicit declaration of function ‘_stricmp’
 */
const char *strcasestr(const char *arg1, const char *arg2) {
	const char *a, *b;

	for(;*arg1;*arg1++) {
		a = arg1;
		b = arg2;
		while((*a++ | 32) == (*b++ | 32))
			if(!*b)
				return (arg1);
	}
	return(NULL);
}
/*
 Borland: strcmpi()
 */
int strcasecmp(const char *s1, const char *s2) {
	while(*s1 != 0 && tolower(*s1) == tolower(*s2)) {
		++s1;
		++s2;
	}
	return (*s2 == 0) ? (*s1 != 0) : (*s1 == 0) ? -1 : (tolower(*s1) - tolower(*s2));
}
#endif

// *INDENT-OFF*
// @formatter:off
static const aiImporterDesc desc = {
		"Videoscape GEO Importer",
		"",
		"",
		"http://paulbourke.net/dataformats/geo/ \
		color settings from: https://home.comcast.net/~erniew/getstuff/geo.html \
		calculation http://home.comcast.net/~erniew/lwsdk/sample/vidscape/surf.c",
		aiImporterFlags_SupportTextFlavour |
		aiImporterFlags_LimitedSupport |
		aiImporterFlags_Experimental,
		0,
		0,
		0,
		0,
		"3DG geo GOUR" /* parsing test */
};
// @formatter:on
// *INDENT-ON*

// ------------------------------------------------------------------------------------------------
// Constructor to be privately used by Importer
GEOImporter::GEOImporter() :
		rgbH(false), pScene(0), tempPositions(0), tempColors(0), fifty_percent(0.01f) {
}

// ------------------------------------------------------------------------------------------------
// Destructor, private as well
GEOImporter::~GEOImporter() {
}

// ------------------------------------------------------------------------------------------------
// Returns whether the class can handle the format of the given file.
bool GEOImporter::CanRead(const std::string& pFile, IOSystem* pIOHandler,
		bool checkSig) const {
	const std::string extension = GetExtension(pFile);

	if (strcasestr(desc.mFileExtensions, extension.c_str()))
		return true;
	else if (!extension.length() || checkSig) {
		if (!pIOHandler)
			return true;
		const char* tokens[] = { "gour", "3dg" }; /* ref: 3dg1 3dg2 3dg3 gour */
		return SearchFileHeaderForToken(pIOHandler, pFile, tokens, ARRAYSIZE(tokens));
	}
	return false;
}

// ------------------------------------------------------------------------------------------------
const aiImporterDesc* GEOImporter::GetInfo() const {
	return &desc;
}

// ------------------------------------------------------------------------------------------------
// Setup configuration properties
void GEOImporter::SetupProperties(const Importer* pImp) {
	DefaultLogger::get()->info(
			(Formatter::format(), "Unsupported operation: ", __FUNCTION__));
}

bool GEOImporter::ValidateFlags(unsigned int pFlags) const {

	pFlags &= ~aiProcess_ValidateDataStructure;
	DefaultLogger::get()->info(
			(Formatter::format(), "Unsupported operation: ", __FUNCTION__));
	return true;
}
// ------------------------------------------------------------------------------------------------
// Imports the given file into the given scene structure.
void GEOImporter::InternReadFile(const std::string& pFile, aiScene* pScene, IOSystem* pIOHandler) {
	boost::scoped_ptr<IOStream> file(pIOHandler->Open(pFile, "rb"));

	// Check whether we can read from the file
	if (file.get() == NULL) {
		throw DeadlyImportError("Failed to open GEO file " + pFile + ".");
	}

	this->pScene = pScene;
	/*std::vector<char> buffer;
	 TextFileToBuffer( file, buffer );
	 GEOParser myParser( buffer );
	 myParser.parse();*/

	// allocate storage and copy the contents of the file to a memory buffer
	std::vector<char> mBuffer2;
	TextFileToBuffer(file.get(), mBuffer2);
	buffer = &mBuffer2[0];

	GetNextLine(buffer, line);
	while ('G' == line[0] || ('3' == line[0] & 'D' == line[1]) || '#' == line[0]) {
		if ('G' == line[0] || ('3' == line[0] & 'D' == line[1]))
			switch (line[3]) {
			case '1':
				flav = Mesh_with_coloured_faces;
				DefaultLogger::get()->debug(
						(Formatter::format(), "Signature: ", line, ", must read color with face data"));
				break;
			case '2':
				flav = Lamp;
			case '3':
				flav = (flav) ? flav : Gouraud_curves_or_NURBS_surfaces;
				DefaultLogger::get()->debug(
						(Formatter::format(), "Signature: ", line, ", must not read any color data, but lights or courves"));
				break;
			case 'R':
				flav = Mesh_with_coloured_vertices;
				DefaultLogger::get()->debug(
						(Formatter::format(), "Signature: ", line, ", must read color with vertex data"));
				break;
			default:
				DefaultLogger::get()->warn(
						(Formatter::format(), "Unknown Signature: ", line));
				throw DeadlyImportError("GEO: Unknown file version");
			}
		GetNextLine(buffer, line); // skip the signature line and comment lines (#...)
	}

	sz = line;
	const unsigned int numElementsToImport = strtoul10(sz, &sz);

	progress->UpdateFileRead(fifty_percent, numElementsToImport);
	//progress->Update(fifty_percent);

	const unsigned int numFaces = 30367;

	pScene->mMeshes = new aiMesh*[pScene->mNumMeshes = 1];
	mesh = pScene->mMeshes[0] = new aiMesh();
	faces = mesh->mFaces = new aiFace[numFaces];

	tempPositions.resize(numElementsToImport);

	if (flav == Mesh_with_coloured_faces)
		GEOImporter::InternReadncV(numElementsToImport);
	else if (flav == Lamp)
		GEOImporter::InternReadLamp(numElementsToImport);
	else if (flav == Gouraud_curves_or_NURBS_surfaces)
		GEOImporter::InternReadFbS(numElementsToImport); // here numElementsToImport is the surface type !!!
	else if (flav == Mesh_with_coloured_vertices)
		GEOImporter::InternReadcV(numElementsToImport);
	else
		throw DeadlyImportError("GEO: Never see me, bug in design.");

	DefaultLogger::get()->debug(
			(Formatter::format(), "GEO: ", __FUNCTION__, " start count faces and calculate total verticesspace"));

	const char* old = buffer;
	int fCaounter = 0;
	if (flav == Mesh_with_coloured_faces
			| flav == Mesh_with_coloured_vertices) {
		// First find out how many vertices we'll need
		while (GetNextLine(buffer, line)) {
			sz = line;
			if (!(faces->mNumIndices = strtoul10(sz, &sz))) {
				DefaultLogger::get()->error(
						"GEO: Faces with zero indices aren't allowed");
				continue;
			}
			mesh->mNumFaces++; // TODO: implement material stuff needs new mesh
			mesh->mNumVertices += faces->mNumIndices;
			faces++;
			//progress->UpdateFileRead(fifty_percent, numElementsToImport);
			progress->Update(fifty_percent);
						fCaounter++;
		}

		if (!mesh->mNumVertices)
			throw DeadlyImportError("GEO: There are no valid faces");

		// shrink the storage space?
		// malloc(mesh->mFaces, sizeof(aiFace*) * fCaounter);
		DefaultLogger::get()->debug((Formatter::format(), "GEO: face storage = ", ARRAYSIZE(mesh->mFaces), " but just need ", fCaounter));
	}

	// allocate storage for the output vertices
	verts = mesh->mVertices = new aiVector3D[mesh->mNumVertices];

	// second: now parse all face indices
	buffer = old;
	faces = mesh->mFaces;

	if (flav != Lamp && flav != Gouraud_curves_or_NURBS_surfaces) {

		// import faces:
		if (flav == Mesh_with_coloured_faces)
			InternReadcF(fCaounter); // mesh colored faces
		//else if (flav == Mesh_with_coloured_vertices)
			//InternReadncF(fCaounter); // mesh colored vertices

		DefaultLogger::get()->debug((Formatter::format(), "GEO: ", __LINE__));

	}
	DefaultLogger::get()->debug((Formatter::format(), "GEO: ", __LINE__));

	InternReadFinish();
}
void GEOImporter::InternReadLamp(int count) {
	DefaultLogger::get()->debug(
			(Formatter::format(), "GEO: Has to import ", count, " light(s)"));

	//delete[] faces;
	//delete pScene->mMeshes[0];

	pScene->mLights = new aiLight*[count];

	while (GetNextLine(buffer, line)) {
		sz = line;
		unsigned int type = strtoul10(sz, &sz);
		char name[16];
		sprintf(name, "Lamp%04d%04X", pScene->mNumLights + 1, type);
		aiString tmpMatName;
		tmpMatName.Set(name);
		aiLight *tmpLight = new aiLight();
		tmpLight->mName = tmpMatName;

		DefaultLogger::get()->debug(
				(Formatter::format(), "GEO: Create light: ", tmpMatName.data));

		//type               - lamp type (0 - point lamp, 1 - spot lamp, 2 - sun)
		tmpLight->mType = (aiLightSourceType) type /*aiLightSource_AMBIENT*/;

		GetNextLine(buffer, line);
		sz = line;
		//spotsize spotblend - size of spot beam in degrees and intensity (length) of beam
		// hm, calculate mAngleInnerCone mAngleOuterCone
		float &ic = tmpLight->mAngleInnerCone;
		float &oc = tmpLight->mAngleOuterCone;
		sz = fast_atoreal_move<float>(sz, (float&) ic);
		SkipSpaces(&sz);
		fast_atoreal_move<float>(sz, (float&) oc);

		GetNextLine(buffer, line);
		sz = line;
		//R G B E            - color (RGB) and (E)nergy of lamp
		// same as diffuse? tmpLight->mColorAmbient = new aiColor3D(1);
		aiColor3D &c = tmpLight->mColorDiffuse;
		sz = fast_atoreal_move<float>(sz, (float&) c.r);
		SkipSpaces(&sz);
		sz = fast_atoreal_move<float>(sz, (float&) c.g);
		SkipSpaces(&sz);
		fast_atoreal_move<float>(sz, (float&) c.b);

		GetNextLine(buffer, line);
		sz = line;
		//x y z              - lamp coordinates
		aiVector3D &p = tmpLight->mPosition;
		sz = fast_atoreal_move<float>(sz, (float&) p.x);
		SkipSpaces(&sz);
		sz = fast_atoreal_move<float>(sz, (float&) p.y);
		SkipSpaces(&sz);
		fast_atoreal_move<float>(sz, (float&) p.z);

		GetNextLine(buffer, line);
		sz = line;
		//vecx vecy vecz     - lamp direction vector
		aiVector3D &d = tmpLight->mDirection;
		sz = fast_atoreal_move<float>(sz, (float&) d.x);
		SkipSpaces(&sz);
		sz = fast_atoreal_move<float>(sz, (float&) d.y);
		SkipSpaces(&sz);
		fast_atoreal_move<float>(sz, (float&) d.z);

		pScene->mLights[pScene->mNumLights] = tmpLight;
		pScene->mNumLights++;
	}
}
void GEOImporter::InternReadFbS(int count) {
	DefaultLogger::get()->debug(
			(Formatter::format(), "GEO: Has to import type ", count, " form(s)"));
	while (GetNextLine(buffer, line)) {
			sz = line;
	}
	throw DeadlyImportError("GEO: not supported yet.");
}
void GEOImporter::InternReadcV(int count) {
	DefaultLogger::get()->debug(
			(Formatter::format(), "GEO: Has to import ", count, " colored vertex/vertices"));

	tempColors.resize(count);
	mesh->mColors[0] = new aiColor4D[count];

	// now read all vertex lines
	for (unsigned int i = 0; i < count; i++) {

		if (!GetNextLine(buffer, line)) {
			DefaultLogger::get()->error(
					"GEO: The number of verts in the header is incorrect");
			break;
		}

		aiVector3D& v = tempPositions[i];

		sz = line;
		sz = fast_atoreal_move<float>(sz, (float&) v.x);
		SkipSpaces(&sz);
		sz = fast_atoreal_move<float>(sz, (float&) v.y);
		SkipSpaces(&sz);
		sz = fast_atoreal_move<float>(sz, (float&) v.z);

		InternReadColor(i);
	}
}
void GEOImporter::InternReadncV(int count) {
	DefaultLogger::get()->debug(
			(Formatter::format(), "GEO: Has to import ", count, " not colored vertex/vertices"));
	// now read all vertex lines
	for (unsigned int i = 0; i < count; i++) {

		if (!GetNextLine(buffer, line)) {
			DefaultLogger::get()->error(
					"GEO: The number of verts in the header is incorrect");
			break;
		}

		aiVector3D& v = tempPositions[i];

		sz = line;
		sz = fast_atoreal_move<float>(sz, (float&) v.x);
		SkipSpaces(&sz);
		sz = fast_atoreal_move<float>(sz, (float&) v.y);
		SkipSpaces(&sz);
		fast_atoreal_move<float>(sz, (float&) v.z);
	}
}
void GEOImporter::InternReadcF(int count) {
	DefaultLogger::get()->debug(
			(Formatter::format(), "GEO: Has to import ", count, " colored face(s)"));
	tempColors.resize(count);
	mesh->mColors[0] = new aiColor4D[mesh->mNumVertices];

	for (unsigned int i = 0, p = 0; i < count;) {
		if (!GetNextLine(buffer, line))
			return;

		sz = line;

		unsigned int idx;
		if (!(idx = strtoul10(sz, &sz)))
			continue;

		faces->mIndices = new unsigned int[faces->mNumIndices = idx];
		for (unsigned int m = 0; m < faces->mNumIndices; m++) {
			SkipSpaces(&sz);
			/*if ((idx = strtoul10(sz, &sz)) >= numElementsToImport) {
				DefaultLogger::get()->error(
						"GEO: Vertex index is out of range");
				idx = numElementsToImport - 1;
			}*/
			idx = strtoul10(sz, &sz);
			faces->mIndices[m] = p++;
			*verts++ = tempPositions[idx];
		}

		InternReadColor(idx);
//
			for (unsigned int l = 0; l < faces->mNumIndices; l++) {
				aiColor4D& col = mesh->mColors[0][faces->mIndices[l]];
				col = tempColors[idx];
			}

		++i;
		++faces;
	}
}
void GEOImporter::InternReadncF(int count) {
	DefaultLogger::get()->debug(
			(Formatter::format(), "GEO: Has to import ", count, " not colored face(s)"));

	mesh->mColors[0] = new aiColor4D[mesh->mNumVertices];

	for (unsigned int i = 0, p = 0; i < mesh->mNumFaces;) {
		if (!GetNextLine(buffer, line))
			break;

		sz = line;

		unsigned int idx;
		if (!(idx = strtoul10(sz, &sz)))
			continue;

		faces->mIndices = new unsigned int[faces->mNumIndices = idx];
		for (unsigned int m = 0; m < faces->mNumIndices; m++) {
			SkipSpaces(&sz);
			/*if ((idx = strtoul10(sz, &sz)) >= numElementsToImport) {
				DefaultLogger::get()->error(
						"GEO: Vertex index is out of range");
				idx = numElementsToImport - 1;
			}*/
			idx = strtoul10(sz, &sz);
			faces->mIndices[m] = p++;
			*verts++ = tempPositions[idx];
			aiColor4D& col = mesh->mColors[0][faces->mIndices[m]];
									col = tempColors[idx];
		}

		++i;
		++faces;
	}
}
void GEOImporter::InternReadFinish() {
	// generate the output node graph
	pScene->mRootNode = new aiNode();
	pScene->mRootNode->mName.Set("<GEORoot>");

	pScene->mRootNode->mMeshes =
			new unsigned int[pScene->mRootNode->mNumMeshes = pScene->mNumMeshes];

	for (unsigned int i = 0; i < pScene->mNumMeshes; i++) {
		pScene->mRootNode->mMeshes[i] = i;
	}
}

void GEOImporter::InternReadColor(int pos) {
	DefaultLogger::get()->debug(
			(Formatter::format(), "GEO: insert color onto position ", pos));
	aiColor4D& c = tempColors[pos];

	// if just one mesh sync with temColors?
	//mesh->mColors[0][pos];

	SkipSpaces(&sz);
	if (!(color = strtoul10(sz, &sz)))
		if (!(color = hexstrtoul10(--sz, &sz))) {
			DefaultLogger::get()->error(
					(Formatter::format(), "GEO: color read failed (sz) ", sz, " color ", color));
			return;
		} else {
			rgbH = true;
		}
	else
		rgbH = false;

	LookupColor(color, c);
}
// ------------------------------------------------------------------------------------------------
void GEOImporter::LookupColor(long iColorIndex, aiColor4D& cOut) {
	if (rgbH) {
		cOut[0] = ((iColorIndex & 0x00ff0000) >> 16) / 255.0f;
		cOut[1] = ((iColorIndex & 0x0000ff00) >> 8) / 255.0f;
		cOut[2] = (iColorIndex & 0x000000ff) / 255.0f;
		cOut[3] = 1.0f;
	} else {
		int index = (iColorIndex & 0x0f);
		cOut = *((const aiColor4D*) (&g_ColorTable[index]));
		//cOut = *(&g_ColorTable[index]);
		//DefaultLogger::get()->debug((Formatter::format(), "Colorindex: ", iColorIndex & 0x0f, " Surface: ", g_Effect[(iColorIndex & 0x30) >> 4], " HiHi:", (iColorIndex & 0xC0)));
		if ((iColorIndex & 0xf0)) {
			DefaultLogger::get()->debug(
					(Formatter::format(), "Achtung! Material required: ", iColorIndex, " Surface: ", g_Effect[(iColorIndex
							& 0x30) >> 4], " HiHi:", (iColorIndex & 0xC0)));

			/*	//pScene->mMaterials = new aiMaterial*[pScene->mNumMaterials];
			 //pScene->mMaterials = (aiMaterial **)realloc(pScene->mMaterials, sizeof(aiMaterial*) * pScene->mNumMaterials);

			 //int x[3] = {1, 2, 3};
			 std::vector<aiMaterial*> v(pScene->mMaterials, pScene->mMaterials + ARRAYSIZE(pScene->mMaterials));
			 v.push_back(pcMat);
			 */
			/*char name[16];
			 sprintf(name, "%08X", (int)iColorIndex);
			 aiString tmpMatName;
			 tmpMatName.Set(name);
			 pcMat->AddProperty(&tmpMatName, AI_MATKEY_NAME);
			 pcMat->AddProperty(&cOut, 1, AI_MATKEY_COLOR_DIFFUSE);
			 pScene->mMaterials[pScene->mNumMaterials] = pcMat;

			 const int twosided = 1;
			 pcMat->AddProperty(&twosided, 1, AI_MATKEY_TWOSIDED);

			 pScene->mNumMaterials++;*/
		}
	}

	//pScene->mNumMeshes++;
	//pScene->mMeshes = (aiMesh **)realloc(&pScene->mMeshes, sizeof(aiMesh*) * pScene->mNumMeshes);
	//	/*aiMesh* mesh =*/ pScene->mMeshes[pScene->mNumMeshes - 1] = new aiMesh();
	// mwsh->mMaterialIndex = thismatindex;
}

#endif // !! ASSIMP_BUILD_NO_GEO_IMPORTER
