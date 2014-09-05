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
#define strcasestr _stricmp
/*
 Windows: stricmp()
 warning: implicit declaration of function ‘_stricmp’
 */      
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

static const aiImporterDesc desc = {
				"Videoscape GEO Importer",
				"",
				"",
				"http://paulbourke.net/dataformats/geo/ \
				color settings from: https://home.comcast.net/~erniew/getstuff/geo.html \
				calculation http://home.comcast.net/~erniew/lwsdk/sample/vidscape/surf.c",
				aiImporterFlags_SupportTextFlavour | aiImporterFlags_LimitedSupport | aiImporterFlags_Experimental,
				0, 0, 0, 0,
				"3DG GEO GOUR" /* parsing test */
};

// ------------------------------------------------------------------------------------------------
// Constructor to be privately used by Importer
GEOImporter::GEOImporter() :
		rgbH(false) {
}

// ------------------------------------------------------------------------------------------------
// Destructor, private as well 
GEOImporter::~GEOImporter() {
}

// ------------------------------------------------------------------------------------------------
void GEOImporter::LookupColor(long iColorIndex, aiColor4D& cOut) {
	if (rgbH) {
        cOut[0] = ((iColorIndex & 0x00ff0000) >> 16) / 255.0f;
        cOut[1] = ((iColorIndex & 0x0000ff00) >> 8) / 255.0f;
        cOut[2] = (iColorIndex & 0x000000ff) / 255.0f;
        cOut[3] = 1.0f;
        DefaultLogger::get()->debug((Formatter::format(), "derived from HexColor: ", iColorIndex, ", r ",cOut[0]," g ",&cOut.g," b ",&cOut.b," a ",cOut[3]));
	} else {
		int index = (iColorIndex & 0x0f);
		//cOut = *((const aiColor4D*) (&g_ColorTable[index]));
		cOut = *(&g_ColorTable[index]);
	}
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
		return SearchFileHeaderForToken(pIOHandler, pFile, tokens,
				ARRAYSIZE(tokens));
	}
	return false;
}

// ------------------------------------------------------------------------------------------------
const aiImporterDesc* GEOImporter::GetInfo() const {
	return &desc;
}

// ------------------------------------------------------------------------------------------------
// Imports the given file into the given scene structure. 
void GEOImporter::InternReadFile(const std::string& pFile, aiScene* pScene,
		IOSystem* pIOHandler) {
	boost::scoped_ptr<IOStream> file(pIOHandler->Open(pFile, "rb"));

	// Check whether we can read from the file
	if (file.get() == NULL) {
		throw DeadlyImportError("Failed to open GEO file " + pFile + ".");
	}

	// allocate storage and copy the contents of the file to a memory buffer
	std::vector<char> mBuffer2;
	TextFileToBuffer(file.get(), mBuffer2);
	const char* buffer = &mBuffer2[0];

	char line[4096];
	GetNextLine(buffer, line);
	while ('G' == line[0] || 'D' == line[1] || '#' == line[0]) {
		if ('G' == line[0] || 'D' == line[1])
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
						(Formatter::format(), "Signature: ", line, ", must not read any color data"));
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

	const char* sz = line;
	SkipSpaces(&sz);
	const unsigned int numVertices = strtoul10(sz, &sz);
	SkipSpaces(&sz);
	const unsigned int numFaces = 42750 /*strtoul10(sz,&sz)*/;
	long lastcolor, color;

	pScene->mMeshes = new aiMesh*[pScene->mNumMeshes = 1];
	aiMesh* mesh = pScene->mMeshes[0] = new aiMesh();
	aiFace* faces = mesh->mFaces = new aiFace[numFaces];

	std::vector<aiVector3D> tempPositions(numVertices);

	// now read all vertex lines
	for (unsigned int i = 0; i < numVertices; i++) {
		if (!GetNextLine(buffer, line)) {
			DefaultLogger::get()->error(
					"GEO: The number of verts in the header is incorrect");
			break;
		}
		aiVector3D& v = tempPositions[i];

		sz = line;
		SkipSpaces(&sz);
		sz = fast_atoreal_move<float>(sz, (float&) v.x);
		SkipSpaces(&sz);
		sz = fast_atoreal_move<float>(sz, (float&) v.y);
		SkipSpaces(&sz);
		fast_atoreal_move<float>(sz, (float&) v.z);

		if(flav == Mesh_with_coloured_vertices){
			SkipSpaces(&sz);
					if (!(color = strtoul10(sz, &sz)))
						if (!(color = hexstrtoul10(--sz, &sz))) {
							DefaultLogger::get()->error(
									(Formatter::format(), "GEO: color read failed (sz) ", sz, " color ", color));
							continue;
						} else {
							rgbH = true;
						}
					else
						rgbH = false;

					aiColor4D& col = mesh->mColors[0][faces->mIndices[i]];
								LookupColor(color, col);
		}
	}

	// First find out how many vertices we'll need
	const char* old = buffer;
	while (GetNextLine(buffer, line)) {
		sz = line;
		SkipSpaces(&sz);
		if (!(faces->mNumIndices = strtoul10(sz, &sz))) {
			DefaultLogger::get()->error(
					"GEO: Faces with zero indices aren't allowed");
			continue;
		}
		mesh->mNumFaces++;
		mesh->mNumVertices += faces->mNumIndices;
		++faces;
	}

	if (!mesh->mNumVertices)
		throw DeadlyImportError("GEO: There are no valid faces");

	// allocate storage for the output vertices
	aiVector3D* verts = mesh->mVertices = new aiVector3D[mesh->mNumVertices];

	aiColor4D* colOut = mesh->mColors[0] =
			new aiColor4D[/*mesh->mNumFaces*/mesh->mNumVertices];

	// second: now parse all face indices
	buffer = old;
	faces = mesh->mFaces;
	for (unsigned int i = 0, p = 0; i < mesh->mNumFaces;) {
		if (!GetNextLine(buffer, line))
			break;

		unsigned int idx;
		sz = line;
		SkipSpaces(&sz);
		if (!(idx = strtoul10(sz, &sz)))
			continue;

		faces->mIndices = new unsigned int[faces->mNumIndices];
		for (unsigned int m = 0; m < faces->mNumIndices; m++) {
			SkipSpaces(&sz);
			if ((idx = strtoul10(sz, &sz)) >= numVertices) {
				DefaultLogger::get()->error(
						"GEO: Vertex index is out of range");
				idx = numVertices - 1;
			}
			faces->mIndices[m] = p++;
			*verts++ = tempPositions[idx];
		}
		SkipSpaces(&sz);
		if (!(color = strtoul10(sz, &sz)))
			if (!(color = hexstrtoul10(--sz, &sz))) {
				DefaultLogger::get()->error(
						(Formatter::format(), "GEO: color read failed (sz) ", sz, " color ", color));
				continue;
			} else {
				rgbH = true;
			}
		else
			rgbH = false;

		if(flav == Mesh_with_coloured_faces)
		for (unsigned int l = 0; l < faces->mNumIndices; l++) {
			aiColor4D& col = mesh->mColors[0][faces->mIndices[l]];

			LookupColor(color, col);
		}

		++i;
		++faces;
	}

	// generate the output node graph
	pScene->mRootNode = new aiNode();
	pScene->mRootNode->mName.Set("<GEORoot>");
//	pScene->mRootNode->mMeshes = new unsigned int [pScene->mRootNode->mNumMeshes = 1];
	pScene->mRootNode->mNumMeshes = 1;
	pScene->mRootNode->mMeshes = new unsigned int[21];

	for (int i = 0; i < 21; i++) {
		pScene->mRootNode->mMeshes[i] = i;
	}

	/*	pScene->mMaterials = new aiMaterial*[21];
	 aiMaterial* pcMat;

	 for (int i = 0; i < 21; i++) {
	 pcMat = new aiMaterial();
	 //aiColor4D clr(1.0f,0.6f,0.6f,1.0f);
	 char *name;
	 sprintf(name, "%d", i);
	 const aiString tmpMatName(name);
	 pcMat->AddProperty(&tmpMatName, AI_MATKEY_NAME);
	 pcMat->AddProperty(&(ColorTable[i]), 1, AI_MATKEY_COLOR_DIFFUSE);
	 pScene->mMaterials[i] = pcMat;
	 }

	 const int twosided = 1;
	 pcMat->AddProperty(&twosided, 1, AI_MATKEY_TWOSIDED);*/
}

#endif // !! ASSIMP_BUILD_NO_GEO_IMPORTER
