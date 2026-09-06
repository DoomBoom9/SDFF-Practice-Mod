#include "pch.h"
#include <d3d9.h>
#include <d3dx9.h>
#include <cmath>
#include <math.h>
#include <algorithm>

ID3DXEffect*& g_pCurrentEffect = *(ID3DXEffect**)0x006CCB00;
D3DXMATRIX*& DAT_00621470 = *(D3DXMATRIX**)0x00621470;
void*& DAT_006CCAEC = *(void**)0x006CCAEC;
void*& DAT_00622460 = *(void**)0x00622460;
void*& DAT_006cb160 = *(void**)0x006cb160;
const char DAT_006b8e68;

struct Vector3 {
	float x;
	float y;
	float z;
};

struct VertexBufferHeader {
	Vector3* dataPtr;
	int vertexCount;
	// Vector3 vertices[8] follows right after in memory
};

struct OBBVertexBuffer {
	VertexBufferHeader header;
	Vector3 vertices[8];
};

struct LargeRenderContext {
	OtherRenderCommand secondaryCommands[0x100]; // need to rename and find out what is
	// object buffer / queue
	RenderCommand commands[0x100]; // 0x2800
	int secondaryCommandObjCount;             // 0x4400
	int queuedObjectCount;             // 0x4404
	RendererInstance* pRendererInstance; // 0x4408
	// remaining fields...
};

// not entirely sure on field names
struct RenderCommand {
	float start[3]; // or Vector3 pos; (offset 0x00 - 0x0b)
	float end[3]; // or Vector3 rot/scale; (offset 0x0c - 0x17)
	uint32_t flags;    // or material/texture ID, pointer (offset 0x18 - 0x1b)
};

// to fill in: all names
// is held at the base of the render context;
struct OtherRenderCommand {
	Vector3 v1;
	Vector3 v2;
	Vector3 v3;
	uint32_t flags;
};

struct ShaderParamEntry {
	DWORD flags;               // +0x00: Status or control flags
	D3DXHANDLE paramHandle;    // +0x04: D3DX effect parameter handle
	void* dataPtr;             // +0x08: Source data pointer (float, vector, matrix, etc.)
	unsigned short paramType;  // +0x0C: Type tag (drives the switch-case dispatch)
	short elementSizeOrCount;  // +0x0E: Array count or size descriptor for matrices/raw blocks
}; // Total stride: 0x10 (16 bytes)

typedef struct HeapBlockHeader {
	void* nextFree;      // 0x00 (Offset -0x10 from memBlock)
	int field_04;        // 0x04 (Offset -0x0c from memBlock)
	int field_08;        // 0x08 (Offset -0x08 from memBlock)
	unsigned int flags;  // 0x0c (Offset -0x04 from memBlock)
	// --- memBlock payload pointer points here (+0x10) ---
} HeapBlockHeader;

typedef struct HeapBin {
	void* freeListHead; // 4 bytes: points to the first free block in this bin
	short itemCount;    // 2 bytes: counter of active/free blocks tracked here
	short padding;      // 2 bytes: alignment padding
} HeapBin;

class RendererInstance {
public:
	struct RenderStateBlock {
		D3DPRIMITIVETYPE currentPrimitiveType;		// 0x00 (0xc9c)
		int vertexOrIndexCount;						// 0x4  (0xca0)
		int primitiveCount;							// 0x8  (0xca4)
		int byteSize;								// 0xc	(0xca8)
		int currentParamIndex;						// 0x10 (0xcac): stores param_2
		void* allocResult;							// 0x14 (0xcb0): unsure about this name
		UINT vertexStride;							// 0x18: (0xcb4) stride used for SetStreamSource
		int vertexCountCopy;						// 0x1c: (0xcb8) unsure about this name
		int statusFlag;								// 0x20: cleared to 0 on switch
		char pad24[0x4];							// 0x24

		struct VBRingState {
			UINT  lockFlags;         // 0x28 (0xcc4)
			UINT  writeOffset;       // 0x2c (0xcc8)
			UINT  bufferSizeBytes;   // 0x30 (0xccc)
			UINT  startVertex;       // 0x34 (0xcd0)
			UINT  byteOffset;        // 0x38 (0xcd4)
			IDirect3DVertexBuffer9* vertexBuffer; // 0x3c (0xcd8)
		}vbRing;

		char pad40[0x18];           // 0x40 - 0x58

		struct Entry {
			IDirect3DVertexDeclaration9* pDecl; // +0x00: (0x58) Vertex declaration pointer
			DWORD unknownFlag;                  // +0x04: (0x5C) Stream tracking or creation flags
			DWORD reserved0;                    // +0x08: (0x60) Reserved / alignment
			void* effectContainer;              // +0x0C: (0x64) Pointer to effect container (holds ID3DXEffect* at +0x08)
			D3DXHANDLE hTechnique;              // +0x10: (0x68) Active technique handle for the shader
			DWORD padding1;                     // +0x14: (0x6C) Padding / alignment field
			int paramCount;                     // +0x18: (0x70) Number of parameter entries in the batch
			ShaderParamEntry* paramEntries;     // +0x1C: (0x74) Pointer to the ShaderParamEntry array
			char unmappedPayload[0x10];         // +0x20 to +0x2F: (0x78 - 0x87) Remaining data up to the 0x30 (48-byte) stride
		} entries[7]; // Total size: 0x30 bytes

		void* activeEntryData;					// 0x1a8: tracks pcVar1 + 4 this ptr is a little dubiously named
		void* activeResourceNode;               // 0x1ac: managed resource pointer updated in FUN_004b1f50

	};

	char padding1[0xa00];             // 0x000 - 0xa00
	IDirect3DDevice9* d3dDevice;      // 0xa00
	char padding2_part1[0x14];        // 0xa04 - 0xa18
	IDirect3DVertexBuffer9* currentVertexBuffer; // 0xa18
	char padding2_part2[0x238];

	// Shader/Constant Handles (0xc4c - 0xc98)
	D3DXHANDLE* hWorldMatrix;               // 0xc4c
	void* hWorldViewMatrix;           // 0xc50
	void* hWorldViewProjMatrix;       // 0xc54
	void* hViewMatrix;                // 0xc58
	void* hInvViewMatrix;             // 0xc5c
	void* hViewProjMatrix;            // 0xc60
	void* hBonePalette;               // 0xc64
	void* hLightAmbient;              // 0xc68
	void* hLightDiffuse;              // 0xc6c
	void* hLightDirection;            // 0xc70
	void* hSHCoefficients;            // 0xc74
	void* hFogParams;                 // 0xc78
	void* hFogColour;                 // 0xc7c
	char pad_c80[4];                  // 0xc80
	void* hLightPositionArray;        // 0xc84
	void* hLightDirectionArray;       // 0xc88
	void* hLightColourArray;          // 0xc8c
	void* hLightDistAttenArray;       // 0xc90
	void* hLightAngAttenArray;        // 0xc94
	void* unk_c98;                    // 0xc98

	RenderStateBlock renderState;	  // 0xc9c
	int profileIndex;					// 0xe4c (4 bytes)
	int currentRenderProfile;         // 0xe50
	char pad_e54[4];                  // 0xe54
	unsigned int stateFlags;          // 0xe58
	char padding4[0x9c];              // 0xe5c - 0xef8 (156 bytes)

	float viewportData[6];            // 0xef8 (24 bytes)
	float defaultViewportBackup[6];   // 0xf10 (24 bytes)

	IDirect3DDevice9* GetDevice() {
		return this->d3dDevice;
	}

	// Example member function utilizing the layout
	int* SetTrackedResource(int* newValue) {
		// Dereferences the pointer stored at offset 0 of the object (this)
		int** slot = (int**)this;

		if (*slot != nullptr) {
			// COM Release (vtable index 2)
			auto** vtable = *(void***)*slot;
			auto releaseFunc = (void(__thiscall*)(void*))vtable[2];
			releaseFunc(*slot);
		}

		*slot = newValue;

		if (newValue != nullptr) {
			// COM AddRef (vtable index 1)
			auto** vtable = *(void***)newValue;
			auto addRefFunc = (void(__thiscall*)(void*))vtable[1];
			addRefFunc(newValue);
		}

		return newValue;
	}
};

void __cdecl SetCollisionDebugMode(RendererInstance *renderer, int mode)

{
	UINT uVar1;

	BatchAndUploadCollisionDebugPrims(renderer);
	if (mode == 1) {
		uVar1 = 0x181;
	}
	else if (mode == 2) {
		uVar1 = 0x881;
	}
	else if (mode == 3) {
		uVar1 = 0x281;
	}
	else if (mode == 4) {
		uVar1 = 0x481;
	}
	else if (mode == 5) {
		uVar1 = 0x81;
	}
	else {
		if (mode != 6) {
			return;
		}
		uVar1 = 0x1081;
	}
	DrawCollisionDebugGeometry(renderer, uVar1);
	return;
}

void __cdecl BatchAndUploadCollisionDebugPrims(undefined4 worldContext)

{
	short* psVar1;
	float* pCollisitonVectorData;
	UINT currentBatchCount;
	UINT currentStartIndex;
	short* pCursorNode;
	undefined renderCommandBuffer[17428];
	float calculatedPositionDelta[3];
	short* pLoopCounter;
	short** pGlobalCollisionManager;
	short* pNextNodeTracker;
	short* pVertexStrideOrCount;
	short primitiveType;

	pGlobalCollisionManager = DAT_006b8e60;
	if (DAT_006b8e60 != (short**)0x0) {
		pCursorNode = *DAT_006b8e60;
		pVertexStrideOrCount = DAT_006b8e60[5];
		pLoopCounter = DAT_006b8e60[3];
		pNextNodeTracker = DAT_006b8e60[8];
		FUN_00443130((int)renderCommandBuffer, worldContext);
		FUN_00443160((int)renderCommandBuffer);
		while (psVar1 = (short*)((int)pLoopCounter + -1), pLoopCounter != (short*)0x0) {
			primitiveType = *pCursorNode;
			if (((primitiveType == 2) || (primitiveType == 3)) ||
				(pLoopCounter = psVar1, primitiveType == 1)) {
				currentBatchCount = (UINT)(ushort)pCursorNode[3];
				currentStartIndex = (UINT)(ushort)pCursorNode[2];
				pLoopCounter = psVar1;
				do {
					pCollisitonVectorData = (float*)(pVertexStrideOrCount + currentStartIndex * 0x20);
					if (primitiveType == 1) {
						FUN_004430c0(calculatedPositionDelta, pCollisitonVectorData, pCollisitonVectorData + 3,
							pCollisitonVectorData[6]);
						FUN_00443280((int)renderCommandBuffer, pCollisitonVectorData, calculatedPositionDelta,
							0xffffffff);
					}
					else if (pCollisitonVectorData[7] == 0.0) {
						FUN_004430c0(calculatedPositionDelta, pCollisitonVectorData, pCollisitonVectorData + 3,
							pCollisitonVectorData[6]);
						FUN_00443280((int)renderCommandBuffer, pCollisitonVectorData, calculatedPositionDelta,
							0xffff0000);
					}
					else {
						FUN_00443280((int)renderCommandBuffer, pCollisitonVectorData, pCollisitonVectorData + 9,
							0xff00ff00);
					}
					currentStartIndex = currentStartIndex + 1 & (int)pNextNodeTracker - 1U;
					currentBatchCount = currentBatchCount - 1;
				} while (currentBatchCount != 0);
				if (primitiveType == 3) {
					*pCursorNode = 0;
				}
			}
			pCursorNode = pCursorNode + 8;
		}
		pLoopCounter = psVar1;
		FUN_004431c0((int)renderCommandBuffer);
	}
	return;
}


/* WARNING: Function: __alloca_probe replaced with injection: alloca_probe */
struct BVHNode {
	float min[3];   // [0..2] 0x0 - 0xc
	float max[3];   // [3..5] 0xc - 0x18
	void* a;        // [6]	  0x1C
	float b;        // [7] — 0.0f sentinel marks "leaf" 0x20?
};

void __cdecl DrawCollisionDebugGeometry(RendererInstance *renderer, UINT mode)

{
	float fVar1;
	float fVar2;
	float fVar3;
	float fVar4;
	float fVar5;
	float fVar6;
	float* pfVar7;
	float fVar8;
	float fVar9;
	float fVar10;
	int iVar11;
	UINT frustrumPlaneMask;
	int local_4cb0;
	USHORT frustPlaneIndex;
	int local_4c98;
	float* apfStack_4c60[511];
	float local_4464;
	int local_4460;
	int* local_445c;
	int* local_4458;
	float *boundingBox; // this is a float[6] for same reason below 
	int* local_443c;
	int local_4438;
	UINT nodeARGB;
	UINT i;
	LargeRenderContext renderContext;
	float** local_18;
	float* local_14;
	int local_10;

	// local c is likely the 
	int local_c;
	undefined4* local_8;

	local_8 = &DAT_006b8d58;

	// local_c = renderer->0xe6c
	// likely camera context
	local_c = FUN_004b0ac0(renderer);

	// likely frustrum base
	iVar11 = FUN_004ba520(local_c);

	local_14 = (float*)(iVar11 + 0x140);

	// then specific planes
	local_10 = local_8[0x14];
	local_443c = (int*)local_8[8];
	local_4438 = local_8[9];
	if ((local_443c != (int*)0x0) && (local_4438 != 0)) {
		// load renderer Instance into renderContext
		FUN_004398c0(&renderContext, renderer);
		// setrenderer state, set streamSource, set vertexdecl, set world matrix, etc.
		FUN_004398f0(&renderContext);
		for (i = 0; i < 2; i++) {
			local_4458 = (&local_443c)[i];
			local_445c = local_4458;
			local_4460 = *local_4458;
			if (*local_4458 != 0) {
				fVar1 = *local_14;
				fVar2 = local_14[1];
				fVar3 = local_14[2];
				fVar4 = local_14[4];
				local_4464 = fVar4;
				fVar5 = local_14[5];
				fVar6 = local_14[6];
				fVar8 = fVar1 - fVar4;
				fVar9 = fVar2 - fVar5;
				fVar10 = fVar3 - fVar6;
				local_4c98 = 1;
				do {
					iVar11 = local_4c98 - 1;
					pfVar7 = apfStack_4c60[local_4c98 - 2];
					if ((((fVar8 < pfVar7[3] != (fVar8 == pfVar7[3])) && (*pfVar7 <= fVar1 + fVar4)) &&
						(fVar9 < pfVar7[4] != (fVar9 == pfVar7[4]))) &&
						(((pfVar7[1] <= fVar2 + fVar5 && (fVar10 < pfVar7[5] != (fVar10 == pfVar7[5]))) &&
							(pfVar7[2] <= fVar3 + fVar6)))) {

						// "pfVar7[7] == 0.0 distinguishes a leaf (single object) node from an internal (two-child) node
						// — the leaf branch extracts and renders geometry, the internal branch pushes both children
						// for further traversal."
						if (pfVar7[7] == 0.0) {
							// the engine is walking a nested hierarchy: 
							// pfVar7 is a pointer to an array of pointers, index 6 grabs a sub-structure (local_18),
							// and index 1 of that sub-structure extracts a pointer to a float array 
							local_18 = (float**)((void**)pfVar7)[6];
							// pfvar7 now points to renderdata obj
							pfVar7 = local_18[1];
							// (*local_18)[0..2] = min corner, (*local_18)[3..5] = max corner
							// local_18 itself is a pointer to a small descriptor struct
							ComputeBoundingBoxCenterAndExtent((float*)&boundingBox, *local_18, *local_18 + 3);
							frustrumPlaneMask = TestAABBFrustum(local_c, boundingBox, boundingBox + 3);
							if (-1 < (int)frustrumPlaneMask) {
								if ((mode & 0x100) == 0) {
									if ((mode & 0x800) == 0) {
										if ((mode & 0x200) == 0) {
											if ((mode & 0x400) == 0) {
												if ((mode & 0x1000) == 0) {
													nodeARGB = (UINT)local_18 | 0xff000000;
												}
												else {
													nodeARGB = (UINT)pfVar7 | 0xff000000;
												}
											}
											else if (pfVar7[5] == (float)(local_10 + -1)) {
												if (*(short*)((int)pfVar7 + 0x12) == -1) {
													nodeARGB = 0xffffffff;
												}
												else {
													nodeARGB = (UINT) * (USHORT*)((int)pfVar7 + 0x12) * 0x382734bd +
														0xfd438374 | 0xff000000;
												}
											}
											else {
												nodeARGB = 0xff404040;
											}
										}
										else if ((*(USHORT*)(pfVar7 + 4) & 1) == 0) {
											nodeARGB = 0xff404040;
										}
										else if ((*(USHORT*)(pfVar7 + 4) & 4) == 0) {
											nodeARGB = 0xffffffff;
										}
										else {
											nodeARGB = 0xff222222;
										}
									}
									else {
										nodeARGB = GetDebugColorByIndex((UINT) * (USHORT*)((int)local_18[2] + 6));
									}
								}
								else {
									nodeARGB = GetDebugColorByIndex((UINT)local_18[4]);
								}
								if ((*(USHORT*)(local_18 + 6) & 1) != 0) {
									nodeARGB = nodeARGB & 0x7fffffff;
								}
								if ((mode & 1) != 0) {
									// this is gonna be a real pain in the ass.
									// DAT_0064fc08 is a ptr to a table of vtables
									// 0x28 is
									/*
										FUN_0043ec70
										FUN_0043d8e0
										FUN_0044f1a0
										FUN_004400c0
									*/
									// Enqueues the given type (normal / dynamic) (sphere, Obb, Convex Mesh, Cylinder)
									(**(code**)(*(int*)(&DAT_0064fc08 + (int)local_18[5] * 4) + 0x28))
										(local_18, renderContext, nodeARGB);
								}
								if ((mode & 2) != 0) {
									EnqueueAABB((float*)&boundingBox, &renderContext, 0xffffffff);
								}
							}
						}

						// child node case
						else {
							apfStack_4c60[local_4c98 + -2] = (float*)pfVar7[6];
							apfStack_4c60[iVar11] = (float*)pfVar7[7];
							iVar11 = local_4c98 + 1;
						}
					}
					local_4c98 = iVar11;
				} while (local_4c98 != 0);
			}
		}
		if ((mode & 0x80) != 0) {
			for (frustPlaneIndex = 0; frustPlaneIndex < 6; frustPlaneIndex++) {
				for (local_4cb0 = FUN_00435d70(frustPlaneIndex); local_4cb0 != 0; local_4cb0 = FUN_00435dd0(local_4cb0)) {
					// NonPenetration / distance / distance anchored / pulley / hinge / cone twist
					(**(code**)(*(int*)(&DAT_0064fb24 + (UINT) * (USHORT*)(local_4cb0 + 0x10) * 4) + 8))
						(local_4cb0, renderContext);
				}
			}
		}
		// wrapper that writes queued obj to the alloc buffer and batches the prims
		// then does the renderpass and ends etc.
		FUN_00439950(&renderContext);

		// streams vertices does render pass etc. but a bit differently than above 
		FUN_00439a30(&renderContext);
	}
	return;
}

void FUN_00443130(int param_1, int param_2) {

}

// FUN_00443160
void FUN_00443160(LargeRenderContext *context) {
	RendererInstance* rendererInstance;

	rendererInstance = context->pRendererInstance;
	SetEngineRenderState(rendererInstance, 0, 1);
	SetEngineRenderState(rendererInstance, 1, 1);
	ApplyPresetRenderStateProfile(rendererInstance, 2);
	SetRenderProfile(rendererInstance, 2);
	FUN_004b3d30(rendererInstance, 0);
	return;
}

void __cdecl SetEngineRenderState(RendererInstance* rendererInstance, int stateType, int value)

{
	IDirect3DDevice9* d3dDevice;

	d3dDevice = rendererInstance->GetDevice();
	if (((rendererInstance->stateFlags & 0x80) != 0) && ((stateType == 0 || (stateType == 1)))) {
		value = 0;
	}
	if (stateType == 0) {
		d3dDevice->SetRenderState((D3DRENDERSTATETYPE)0x17, 8);
	}
	else if (stateType == 1) {
		d3dDevice->SetRenderState((D3DRENDERSTATETYPE)0xe, value);
	}
	else if (stateType == 7) {
		d3dDevice->SetRenderState((D3DRENDERSTATETYPE)0x17, 8);
		d3dDevice->SetRenderState((D3DRENDERSTATETYPE)0xe, 0);
	}
	if (value == 0) {
		rendererInstance->stateFlags = ~(1 << ((byte)stateType & 0x1f)) & rendererInstance->stateFlags;
	}
	else {
		rendererInstance->stateFlags = 1 << ((byte)stateType & 0x1f) | rendererInstance->stateFlags;
	}
	return;
}

void __cdecl ApplyPresetRenderStateProfile(RendererInstance* rendererInstance, int profileIndex)

{
	IDirect3DDevice9* d3dDevice;

	d3dDevice = rendererInstance->GetDevice();
	if ((profileIndex == 5) || ((rendererInstance->stateFlags & 0x80) == 0)) {
		switch (profileIndex) {
		case 0:
			d3dDevice->SetRenderState(D3DRS_BLENDOP, 1);
			d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 0);
			d3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, 0);
			break;
		case 1:
			d3dDevice->SetRenderState(D3DRS_BLENDOP, 1);
			d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 0);
			d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
			d3dDevice->SetRenderState(D3DRS_ALPHAREF, 0x7f);
			d3dDevice->SetRenderState(D3DRS_ALPHAFUNC, 5);
			break;
		case 2:
			d3dDevice->SetRenderState(D3DRS_BLENDOP, 1);
			d3dDevice->SetRenderState(D3DRS_SRCBLEND, 5);
			d3dDevice->SetRenderState(D3DRS_DESTBLEND, 6);
			d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
			d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
			d3dDevice->SetRenderState(D3DRS_ALPHAREF, 0);
			d3dDevice->SetRenderState(D3DRS_ALPHAFUNC, 5);
			break;
		case 3:
			d3dDevice->SetRenderState(D3DRS_BLENDOP, 1);
			d3dDevice->SetRenderState(D3DRS_SRCBLEND, 5);
			d3dDevice->SetRenderState(D3DRS_DESTBLEND, 2);
			d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
			d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
			d3dDevice->SetRenderState(D3DRS_ALPHAREF, 0);
			d3dDevice->SetRenderState(D3DRS_ALPHAFUNC, 5);
			break;
		case 4:
			d3dDevice->SetRenderState(D3DRS_BLENDOP, 3);
			d3dDevice->SetRenderState(D3DRS_SRCBLEND, 2);
			d3dDevice->SetRenderState(D3DRS_DESTBLEND, 2);
			d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
			d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
			d3dDevice->SetRenderState(D3DRS_ALPHAREF, 0);
			d3dDevice->SetRenderState(D3DRS_ALPHAFUNC, 5);
			break;
		case 5:
			d3dDevice->SetRenderState(D3DRS_BLENDOP, 1);
			d3dDevice->SetRenderState(D3DRS_SRCBLEND, 1);
			d3dDevice->SetRenderState(D3DRS_DESTBLEND, 2);
			d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
			d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
			d3dDevice->SetRenderState(D3DRS_ALPHAREF, 0);
			d3dDevice->SetRenderState(D3DRS_ALPHAFUNC, 5);
			break;
		case 6:
			d3dDevice->SetRenderState(D3DRS_BLENDOP, 1);
			d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
			d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
			d3dDevice->SetRenderState(D3DRS_ALPHAREF, 0);
			d3dDevice->SetRenderState(D3DRS_ALPHAFUNC, 5);
			d3dDevice->SetRenderState(D3DRS_SRCBLEND, 9);
			d3dDevice->SetRenderState(D3DRS_DESTBLEND, 1);
			break;
		case 7:
			d3dDevice->SetRenderState(D3DRS_BLENDOP, 1);
			d3dDevice->SetRenderState(D3DRS_SRCBLEND, 2);
			d3dDevice->SetRenderState(D3DRS_DESTBLEND, 6);
			d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
			d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
			d3dDevice->SetRenderState(D3DRS_ALPHAREF, 0);
			d3dDevice->SetRenderState(D3DRS_ALPHAFUNC, 5);
		}
	}
	else {
		d3dDevice->SetRenderState(D3DRS_BLENDOP, 1);
		d3dDevice->SetRenderState(D3DRS_SRCBLEND, 5);
		d3dDevice->SetRenderState(D3DRS_DESTBLEND, 2);
		d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
		d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
		d3dDevice->SetRenderState(D3DRS_ALPHAREF, 0);
		d3dDevice->SetRenderState(D3DRS_ALPHAFUNC, 5);
	}
	rendererInstance->profileIndex = profileIndex;
	return;
}

// FUN_004b1840
void __cdecl SetRenderProfile(RendererInstance* renderer, int renderProfile)

{
	IDirect3DDevice9* d3dDevice;

	renderer->currentRenderProfile = renderProfile;
	d3dDevice = renderer->GetDevice();
	d3dDevice->SetRenderState((D3DRENDERSTATETYPE)0x16, *(int*)(0x00622e30 + renderProfile * 4));
	return;
}



static int __fastcall FUN_004b5520(int* param_1) {
	return *param_1;
}

// unfinished
// FUN_004b3d30
void __cdecl FUN_004b3d30(RendererInstance* renderInstance, int entryIndex)

{
	RendererInstance::RenderStateBlock::Entry *entry;
	IDirect3DVertexBuffer9* pStreamData;
	IDirect3DVertexDeclaration9* pDecl;
	IDirect3DDevice9* d3dDevice;

	if (entryIndex < 7) {
		entry = &(renderInstance->renderState).entries[entryIndex];
		RenderInstance_SetMatrix(renderInstance->hWorldMatrix, DAT_00621470);
		(renderInstance->renderState).statusFlag = 0;
		// this dat likely an entry?
		(renderInstance->renderState).vertexStride = *(UINT*)(&DAT_00622460 + entryIndex * 0x30);
		d3dDevice = renderInstance->GetDevice();
		pDecl = FUN_004b5760(entry);
		d3dDevice->SetVertexDeclaration(pDecl);
		FUN_004b56e0(renderInstance->padding2_part2, entry);

		// skip past the header of entry to go straight to the data
		(renderInstance->renderState).activeEntryData = (char *)entry + 4;
		if (entryIndex == 0) {
			FUN_004b1f50(renderInstance, 0);
		}
		pStreamData = (renderInstance->renderState).vbRing.vertexBuffer;
		d3dDevice = renderInstance->GetDevice();
		d3dDevice->SetStreamSource(0,pStreamData, 0, (renderInstance->renderState).vertexStride);
		SafeComAssign(&renderInstance->currentVertexBuffer, pStreamData);
	}
	else {
		(renderInstance->renderState).vertexStride = 0;
	}
	(renderInstance->renderState).currentParamIndex = entryIndex;
	return;
}

// may not be accurate was a ai assisted fn
// FUN_004b5670
template <typename T>
inline void SafeComAssign(T** slot, T* newValue) {
	if (*slot != nullptr) {
		(*slot)->Release();
	}
	*slot = newValue;
	if (*slot != nullptr) {
		(*slot)->AddRef();
	}
}

// FUN_004b1f50
// resource node destructor?
void __cdecl FUN_004b1f50(RendererInstance* renderer, void* param_2)

{
	// I assume this destroys the active resource node and its children
	if ((renderer->renderState).currentParamIndex < 7) {
		*(void**)((renderer->renderState).entries[(renderer->renderState).currentParamIndex].entryData + 0x24) = param_2;
		if ((renderer->renderState).activeResourceNode != nullptr) {
			FUN_004ad650((int**)(renderer->renderState).activeResourceNode);
		}
		(renderer->renderState).activeResourceNode = param_2;
		if ((renderer->renderState).activeResourceNode != nullptr) {
			FUN_004ad610((int**)(renderer->renderState).activeResourceNode);
		}
	}
	else {
		if ((renderer->renderState).activeResourceNode != nullptr) {
			FUN_004ad650((int**)(renderer->renderState).activeResourceNode);
		}
		(renderer->renderState).activeResourceNode = nullptr;
	}
	return;
}

// FUN_004c2b60
bool __cdecl RenderInstance_SetMatrix(D3DXHANDLE *hParam, D3DXMATRIX *pMatrix) {
	HRESULT result;
	bool bVar2;

	// could potentially be another d3dx ptr
	if (DAT_006CCAEC == 0) {
		bVar2 = false;
	}
	else {
		HRESULT result = g_pCurrentEffect->SetMatrix(*hParam, pMatrix);
		bVar2 = -1 < result;
	}
	return bVar2;
}

IDirect3DVertexDeclaration9* __fastcall FUN_004b5760(RendererInstance::RenderStateBlock::Entry* param_1) {
	return *param_1;
}

void* __thiscall FUN_004b56e0(void* this, int** param_2) {
	FUN_004b5710(this, *param_2);
	return this;
}

int* __thiscall FUN_004b5710(void* this, int* param_2) {
	return param_2;
}

void FUN_004b1f40(int param_1, int param_2) {

}

void __cdecl FUN_004ad650(int** activeResourceNode)

{
	if (activeResourceNode != nullptr) {
		activeResourceNode[7] = (int*)((int)activeResourceNode[7] + -1);
		FUN_004ad4f0(*activeResourceNode);
	}
	return;
}

void __cdecl FUN_004ad4f0(int* param_1)

{
	int iVar1;
	UINT i;

	if (*param_1 == 1) {
		FUN_004ad5a0((int)param_1);
		if (param_1[3] != 0) {
			for (i = 0; i < (UINT)param_1[1]; i++) {
				iVar1 = i * 0x50 + param_1[3];
				// I assume this is a destructor
				(**(code**)(**(int**)(iVar1 + 4) + 8))(*(uint32_t*)(iVar1 + 4));
				*(undefined4*)(iVar1 + 4) = 0;
			}
		}
		if (param_1[2] != 0) {
			Heap_Free((undefined*)param_1);
		}
	}
	else {
		*param_1 = *param_1 + -1;
	}
	return;
}


// does not instruction match it's just to visually see
void Heap_Free(void* memBlock) {

	HeapBin* targetBin;
	HeapBlockHeader* header;


	if ((memBlock != &DAT_006b8e68) && (memBlock != nullptr)) {
		// Check if the allocation block flag bit 0 is cleared (indicating a free or linked-list managed block)
		header = (HeapBlockHeader*)((char *)memBlock - 0x10);
		if ((header->flags & 1) == 0) {
			Heap_MergeFreeBlock(header);
			return;
		}
		targetBin = ((HeapBin*)header->nextFree + ((((header->flags >> 1) & 0x7fff) + 2) * 12));
		header->nextFree = targetBin->freeListHead;
		targetBin->freeListHead = header;
		targetBin->itemCount--;
	}
	return;
}


// this does not instruction match it's just to know what's happening
void __stdcall Heap_MergeFreeBlock(HeapBlockHeader* block)

{
	char *pcVar1;
	HeapBlockHeader** ppiVar2;
	int piVar3;
	HeapBlockHeader** ppiVar4;
	HeapBlockHeader** ppiVar5;
	HeapBlockHeader** ppiVar6;

	pcVar1 = (char *)block->nextFree;
	ppiVar2 = *(HeapBlockHeader ***)(pcVar1 + 0x6c);
	piVar3 = block->field_08;
	ppiVar4 = (HeapBlockHeader **)((char *)block - ((unsigned int)block->flags >> 1 & 0x7fff));
	ppiVar4[1] = (HeapBlockHeader *)piVar3;
	*(int*)(pcVar1 + 4) = *(int*)(pcVar1 + 4) + piVar3;
	ppiVar6 = nullptr;
	if (ppiVar2 != nullptr) {
		do {
			ppiVar5 = ppiVar2;
			if (ppiVar4 <= ppiVar5) break;
			ppiVar2 = (HeapBlockHeader **)*ppiVar5;
			ppiVar6 = ppiVar5;
		} while (*ppiVar5 != nullptr);
		if (ppiVar6 != nullptr) {
			if (ppiVar4 == (HeapBlockHeader **)((char *)ppiVar6[1] + (int)ppiVar6)) {
				ppiVar6[1] = (HeapBlockHeader *)((char *)ppiVar4[1] + (int)ppiVar6[1]);
				ppiVar4 = ppiVar6;
			}
			else {
				*ppiVar4 = *ppiVar6;
				*ppiVar6 = (HeapBlockHeader*)ppiVar4;
			}
			goto LAB_00457c9a;
		}
	}
	*ppiVar4 = *(HeapBlockHeader**)(pcVar1 + 0x6c);
	*(HeapBlockHeader***)(pcVar1 + 0x6c) = ppiVar4;
LAB_00457c9a:
	ppiVar2 = (HeapBlockHeader**)*ppiVar4;
	if (ppiVar2 == (HeapBlockHeader**)((int)ppiVar4[1] + (int)ppiVar4)) {
		ppiVar4[1] = (HeapBlockHeader*)((int)ppiVar2[1] + (int)ppiVar4[1]);
		*ppiVar4 = *ppiVar2;
	}
	return;
}



void FUN_004ad610(int** param_1) {
	if (param_1 != nullptr) {
		param_1[7] = (int*)((int)param_1[7] + 1);
		**param_1 = **param_1 + 1;
	}
}

void FUN_004430c0(float* param_1, float* param_2, float* param_3, float param_4) {

}

void FUN_004430c0(int param_1, int* param_2, int* param_3, int param_4) {

}

void FUN_004431c0(int param_1) {

}

// returns success of vertexData and appendage to the rendererInstance
// done
// FUN_004b3e70
int __cdecl PreparePrimitiveBatch(RendererInstance* pRendererInstance, int param_2, int queuedObjectCount)

{
	UINT vertexStride;
	void* vertexDataBuff;
	D3DPRIMITIVETYPE primitiveType;
	UINT primitiveCount;
	UINT vertexCount;

	switch (param_2) {
	case 0:
		primitiveType = D3DPT_POINTLIST;
		vertexCount = queuedObjectCount;
		primitiveCount = queuedObjectCount;
		break;
	case 1:
		primitiveType = D3DPT_LINELIST;
		vertexCount = queuedObjectCount << 1;
		primitiveCount = queuedObjectCount;
		break;
	case 2:
		primitiveType = D3DPT_LINESTRIP;
		vertexCount = queuedObjectCount + 1;
		primitiveCount = queuedObjectCount;
		break;
	case 3:
		primitiveType = D3DPT_TRIANGLELIST;
		vertexCount = queuedObjectCount * 3;
		primitiveCount = queuedObjectCount;
		break;
	case 4:
		primitiveType = D3DPT_TRIANGLESTRIP;
		vertexCount = queuedObjectCount + 2;
		primitiveCount = queuedObjectCount;
		break;
	case 5:
		primitiveType = D3DPT_TRIANGLEFAN;
		vertexCount = queuedObjectCount + 2;
		primitiveCount = queuedObjectCount;
		break;
	case 6:
		primitiveType = D3DPT_TRIANGLELIST;
		vertexCount = queuedObjectCount * 6;
		primitiveCount = queuedObjectCount << 1;
		break;
	case 7:
	case 8:
		primitiveType = D3DPT_TRIANGLELIST;
		vertexCount = queuedObjectCount * 6;
		primitiveCount = queuedObjectCount << 1;
		break;
	default:
		primitiveType = D3DPT_POINTLIST;
		vertexCount = queuedObjectCount;
		primitiveCount = queuedObjectCount;
	}
	if (vertexCount < 0xffff) {
		vertexStride = (pRendererInstance->renderState).vertexStride;
		vertexDataBuff = AllocAndLockVertexBuffer(&(pRendererInstance->renderState).vbRing, vertexStride, vertexCount);
		(pRendererInstance->renderState).allocResult = vertexDataBuff;
		if ((pRendererInstance->renderState).allocResult != nullptr) {
			(pRendererInstance->renderState).pad24[0] = param_2;
			(pRendererInstance->renderState).currentPrimitiveType = primitiveType;
			(pRendererInstance->renderState).primitiveCount = primitiveCount;
			(pRendererInstance->renderState).byteSize = vertexStride * vertexCount;
			(pRendererInstance->renderState).vertexOrIndexCount = vertexCount;
			(pRendererInstance->renderState).vertexCountCopy = vertexCount;
			pRendererInstance->padding4[0] |= 8;
			return 1;
		}
	}
	return 0;
}

// vbRing stands for vertex buffer ring
// its like the wrapper for this
// contains info on how things should be written as well as the vertex buffer interface

// this fn returns pointer to a memory buffer containing the returned vertex data.
// FUN_004b3bf0
void* AllocAndLockVertexBuffer(RendererInstance::RenderStateBlock::VBRingState* vbRing, unsigned int vertexStride, int vertexCount)
{
	HRESULT hres;
	UINT totalSizeBytes;
	UINT writeOffset;
	DWORD flags;
	void* pBuffOut;
	UINT buffSize;

	pBuffOut = nullptr;
	buffSize = vbRing->bufferSizeBytes;
	totalSizeBytes = vertexStride * vertexCount;
	if ((vbRing->vertexBuffer != nullptr) && (totalSizeBytes <= buffSize)) {
		flags = vbRing->lockFlags;
		writeOffset = vbRing->writeOffset;
		if (writeOffset % vertexStride != 0) {
			writeOffset = (vertexStride - writeOffset % vertexStride) + writeOffset;
		}
		if (buffSize < writeOffset + totalSizeBytes) {
			writeOffset = 0;
			flags = D3DLOCK_DISCARD | D3DLOCK_NOSYSLOCK;
		}
		hres = vbRing->vertexBuffer->Lock(writeOffset, totalSizeBytes, &pBuffOut, flags);

		// some failure
		if (-1 < hres) {
			vbRing->lockFlags = D3DLOCK_NOOVERWRITE | D3DLOCK_NOSYSLOCK;
			vbRing->startVertex = writeOffset / vertexStride;
			vbRing->byteOffset = writeOffset;
			vbRing->writeOffset = writeOffset + totalSizeBytes;
		}
	}
	// pointer to a memory buffer containing the returned vertex data.
	return pBuffOut;
}

// write something to allcoResult buffer that is allocated for the gpu to read and draw vertices.
void __cdecl
FUN_004b41e0(RendererInstance* pRendererInstance, Vector3* v1, Vector3* v2, Vector3* v3,
	int param_5, int param_6, int param_7)
{
	uint32_t* puVar1;

	puVar1 = (uint32_t *)pRendererInstance->renderState.allocResult;
	pRendererInstance->renderState.allocResult = puVar1 + 0xc;
	pRendererInstance->renderState.vertexCountCopy -= 3;

	// remember that allocResult contains the memory to the vertexBuffer that gets allocated to be read by the gpu
	*puVar1 = v1->x;
	puVar1[1] = v1->y;
	puVar1[2] = v1->z;
	puVar1[3] = param_5;
	puVar1[4] = v2->x;
	puVar1[5] = v2->y;
	puVar1[6] = v2->z;
	puVar1[7] = param_6;
	puVar1[8] = v3->x;
	puVar1[9] = v3->y;
	puVar1[10] = v3->z;
	puVar1[0xb] = param_7;
	return;
}

// FUN_004b4060
void __cdecl StreamVertex(RendererInstance* pRendererInstance, float* vector3, uint32_t flag) {
	uint32_t* pBase;
	// this is vertex data populated by IDirect3DVertexBuffer9::Lock()
	pBase = (uint32_t*)(pRendererInstance->renderState).allocResult;
	(pRendererInstance->renderState).allocResult = pBase + 4;
	(pRendererInstance->renderState).vertexCountCopy--;

	// this writes directly to the IDirect3DVertexBuffer9::Lock() buffer which will be handed
	// to the GPU when the buffer is unlocked and the draw call happens.
	*pBase = *vector3;
	pBase[1] = vector3[1];
	pBase[2] = vector3[2];
	pBase[3] = flag;
	return;
}

// TODO
// Unlocks vertex buff, processes active entry, does the effect pass for active effects
// draws prims to backbuffer, ends, resets renedererState
void __cdecl FUN_004b5140(RendererInstance* pRendererInstance)

{
	void* activeEntryData;
	UINT requiredRenderPasses;
	IDirect3DDevice9* d3dDevice;

	UnlockVertexBuffer(&(pRendererInstance->renderState).vbRing);
	activeEntryData = (pRendererInstance->renderState).activeEntryData;

	// process activeEntryEffect params
	requiredRenderPasses = FUN_004c21f0(activeEntryData);
	if (requiredRenderPasses != 0) {
		// Effect pass for active effects
		FUN_004c2490(activeEntryData, 0);
		DAT_006cb160 = DAT_006cb160 + 1;
		if (
			((pRendererInstance->renderState).currentPrimitiveType == 6) ||
			((pRendererInstance->renderState).currentPrimitiveType) ||
			((pRendererInstance->renderState).currentPrimitiveType == 5)
		) {
			DAT_006cb164 = DAT_006cb164 + (pRendererInstance->renderState).primitiveCount;
		}
		d3dDevice = pRendererInstance->GetDevice();
		// draw primitive to the graphics mem back buffer
		d3dDevice->DrawPrimitive(
			(pRendererInstance->renderState).currentPrimitiveType,
			(pRendererInstance->renderState).vbRing.startVertex, 
			(pRendererInstance->renderState).primitiveCount
		);
		EndActiveEffectPass(activeEntryData);
		EndActiveTechnique(activeEntryData);
	}
	// reset renderer state
	*(UINT*)(pRendererInstance + 0xe5c) = *(UINT*)(pRendererInstance + 0xe5c) & 0xfffffff7;
	pRendererInstance->renderState.currentPrimitiveType = D3DPT_FORCE_DWORD;
	pRendererInstance->renderState.primitiveCount = 0;
	pRendererInstance->renderState.byteSize = 0;
	pRendererInstance->renderState.allocResult = nullptr;
	return;
}

// FUN_004b3ce0
// done
HRESULT __cdecl UnlockVertexBuffer(RendererInstance::RenderStateBlock::VBRingState* vbRing)

{
	HRESULT hres = S_OK;

	if (vbRing->vertexBuffer != nullptr) {
		hres = vbRing->vertexBuffer->Unlock();
	}
	return hres;
}
// this function Sets the rendering technique if the technique is different to the active One
// Then it iterates through the parameters in the effect's active entry and sets them based on type
// returns num of passes it takes to render current technique
// 
// call it something like ProcessRenderEffectEntry
// FUN_004c21f0
UINT __cdecl FUN_004c21f0(void* pActiveEntryData)

{
	short count;
	void* pShaderEntryData;
	D3DXHANDLE hParameter;
	ShaderParamEntry* pParamEntries;
	int paramCount;
	UINT pPasses;
	HRESULT hres;
	D3DXHANDLE hTechnique;
	int local_14;
	ID3DXEffect* pEffect;
	void* pEffectContainer;

	// genuinely could not tell you what this is except it has something to do with the
	// rendering and we're passing in the hParamEntries
	uint32_t* local_8;
	
	local_8 = &DAT_006ccae8;
	
	pEffectContainer = *(void**)((char*)pActiveEntryData + 8);
	// this should be cleaned up later
	if ((pEffectContainer != nullptr) && (pEffect = *(ID3DXEffect**)((char*)pEffectContainer + 8), pEffect != nullptr)) {
		local_14 = DAT_006ccae8;
		hTechnique = *(D3DXHANDLE*)((char*)pActiveEntryData + 0xc);
		hres = 0;

		// if active technique is not equal to current technique?
		if (DAT_006ccae8 != hTechnique) {
			DAT_006ccae8 = hTechnique;
			hres = pEffect->SetTechnique(hTechnique);
		}
		if (-1 < hres) {
			paramCount = *(int*)((char*)pActiveEntryData + 0x14);
			pParamEntries = (ShaderParamEntry*)((char*)pActiveEntryData + 0x18);
			local_8[3] = 0;
			while (paramCount != 0) {
				pShaderEntryData = pParamEntries->dataPtr;
				if (pShaderEntryData != nullptr) {
					count = pParamEntries->elementSizeOrCount;
					hParameter = pParamEntries->paramHandle;
					if (hParameter != nullptr) {
						switch (pParamEntries->paramType) {
						case 0:
							// idk what the hell this is.
							local_8[local_8[3] + 0x944] = pParamEntries;
							local_8[3] = local_8[3] + 1;
							break;
						case 1:
							pEffect->SetFloat(hParameter, *(float*)pShaderEntryData);
							break;
						case 2:
							pEffect->SetVector(hParameter, (D3DXVECTOR4*)pShaderEntryData);
							break;
						case 3:
							if (count == 0) {
								pEffect->SetMatrix(hParameter, (D3DXMATRIX*)pShaderEntryData);
							}
							else {
								pEffect->SetMatrixArray(hParameter, (D3DXMATRIX*)pShaderEntryData, count);
							}
							break;
						case 4:
							pEffect->SetValue(hParameter, pShaderEntryData, count);
						}
					}
				}
				pParamEntries = pParamEntries + 0x10;
				paramCount = paramCount + -1;
			}
			pPasses = 0;
			hres = pEffect->Begin(&pPasses, D3DXFX_DONOTSAVESTATE);
			if (-1 < hres) {
				return pPasses;
			}
		}
	}
	return 0;
}

// Activates ID3DXEffect pass (shaders renderstates etc.)
// Walks active texture stages and see if the texture or sample stages have changed
// If they have call set tex/sample state
// 
// TLDR: commit this shader pass and make sure the GPU's texture/sampler bindings match what the pass expects
// TODO
void __cdecl FUN_004c2490(void* pActiveEntryData, UINT pass)
{
	ID3DXEffect* pEffect;
	int iVar2;
	DWORD uVar3;
	IDirect3DDevice9* d3dDevice;
	UINT Stage;
	int iVar4;
	int iVar5;
	int local_1c;
	int* local_c;

	pEffect = *(ID3DXEffect**)((char*)*(void**)((char*)pActiveEntryData + 8) + 8);
	pEffect->BeginPass(pass);
	pEffect->CommitChanges();
	local_1c = DAT_006ccaf4;
	local_c = &DAT_006ceff8;
	// this points to a global rendererInstance
	d3dDevice = &PTR_006cf3fc->GetDevice();
	// this is the actual thing that needs to be cleaned up and uncommented while (iVar5 = local_1c + -1, local_1c != 0) {
	// fake one to see intellisense
	while (true) {
		iVar4 = *local_c;
		local_c = local_c + 1;
		iVar2 = *(int*)(iVar4 + 8);
		Stage = (UINT) * (ushort*)(iVar4 + 0xe);
		iVar4 = Stage * 0x3c;

		// IVar2 likely a ptr to a struct where 
		/*
				void*                   unknown0;   // +0x00
				IDirect3DBaseTexture9*  texture;    // +0x04  actual device texture object
				DWORD                   addressU;   // +0x08
				DWORD                   addressV;   // +0x0C
				DWORD                   minFilter;  // +0x10
				DWORD                   magFilter;  // +0x14
				DWORD                   mipFilter;  // +0x18
			
		*/

		// DAT_006cf038 The Global State Cache Array probably
		if (*(int*)(&DAT_006cf038 + iVar4) != iVar2) {
			d3dDevice->SetTexture(Stage, *(IDirect3DBaseTexture9**)(iVar2 + 4));
			*(int*)(&DAT_006cf038 + iVar4) = iVar2;
		}
		uVar3 = *(ulong*)(iVar2 + 8);
		if (*(ulong*)(&DAT_006cf040 + iVar4) != uVar3) {
			*(ulong*)(&DAT_006cf040 + iVar4) = uVar3;
			d3dDevice->SetSamplerState(Stage, D3DSAMP_ADDRESSU, uVar3);
		}
		uVar3 = *(ulong*)(iVar2 + 0xc);
		if (*(ulong*)(&DAT_006cf044 + iVar4) != uVar3) {
			*(ulong*)(&DAT_006cf044 + iVar4) = uVar3;
			d3dDevice->SetSamplerState(Stage, D3DSAMP_ADDRESSV, uVar3);
		}
		uVar3 = *(ulong*)(iVar2 + 0x10);
		if (*(ulong*)(&DAT_006cf054 + iVar4) != uVar3) {
			*(ulong*)(&DAT_006cf054 + iVar4) = uVar3;
			// Sampler // Filter // Value
			d3dDevice->SetSamplerState(Stage, D3DSAMP_MINFILTER, uVar3);
		}
		uVar3 = *(ulong*)(iVar2 + 0x14);
		if (*(ulong*)(&DAT_006cf050 + iVar4) != uVar3) {
			*(ulong*)(&DAT_006cf050 + iVar4) = uVar3;
			d3dDevice->SetSamplerState(Stage, D3DSAMP_MAGFILTER, uVar3);
		}
		uVar3 = *(ulong*)(iVar2 + 0x18);
		local_1c = iVar5;
		if (*(ulong*)(&DAT_006cf058 + iVar4) != uVar3) {
			*(ulong*)(&DAT_006cf058 + iVar4) = uVar3;
			d3dDevice->SetSamplerState(Stage, D3DSAMP_MIPFILTER, uVar3);
		}
	}
	return;
}

// FUN_004c2710
void __cdecl EndActiveEffectPass(void* pActiveEntryData)

{
	ID3DXEffect* pEffect;
	// likely pEffect same as FUN_004c2490
	pEffect = *(ID3DXEffect**)(*(int*)((char*)pActiveEntryData + 8) + 8);
	pEffect->EndPass();
	return;
}

// FUN_004c2450
// The effect system responds by using the state block created when ID3DXEffect::Begin was called,
// to automatically restore the pipeline state before ID3DXEffect::Begin.
void __cdecl EndActiveTechnique(void* pActiveEntryData)

{
	ID3DXEffect* pEffect;

	pEffect = *(ID3DXEffect**)(*(int*)((char*)pActiveEntryData + 8) + 8);
	pEffect->End();
	return;
}

void __cdecl FUN_00443310(int* param_1, int* param_2) {

}

// FUN_004b0ac0
int __cdecl FUN_004b0ac0(RendererInstance *renderer)
{
	int local_8;

	if (renderer == 0) {
		local_8 = 0;
	}
	else {
		local_8 = *(int*)(renderer + 0xe6c);
	}
	return local_8;
}

int __cdecl FUN_004ba520(int param_1)

{
	return param_1 + 0x100;
}

void __cdecl FUN_004398c0(LargeRenderContext *pContext, RendererInstance *renderer)

{
	pContext->secondaryCommandObjCount = 0;
	pContext->queuedObjectCount = 0;
	pContext->pRendererInstance = renderer;
	return;
}

// FUN_004398f0
// Identical to 00443160
void __cdecl FUN_004398f0(LargeRenderContext *context) {
	RendererInstance* rendererInstance;

	rendererInstance = context->pRendererInstance;
	SetEngineRenderState(rendererInstance, 0, 1);
	SetEngineRenderState(rendererInstance, 1, 1);
	ApplyPresetRenderStateProfile(rendererInstance, 2);
	SetRenderProfile(rendererInstance, 2);
	FUN_004b3d30(rendererInstance, 0);

	return;
}

// FUN_00439750
void __cdecl ComputeBoundingBoxCenterAndExtent(float* pOut, float* pMin, float* pMax)

{
	float centreX;
	float centreY;
	float centreZ;

	centreX = (pMin[0] + pMax[0]) * 0.5;
	centreY = (pMin[1] + pMax[1]) * 0.5;
	centreZ = (pMin[2] + pMax[2]) * 0.5;
	
	// store centre
	pOut[0] = centreX;
	pOut[1] = centreY;
	pOut[2] = centreZ;

	// store BB radius
	pOut[3] = pMax[0] - centreX;
	pOut[4] = pMax[1] - centreY;
	pOut[5] = pMax[2] - centreZ;
	return;
}

// FUN_004bc4f0
// Check if param_1 actually is the frustum context and if 0x100 contains the planes
/* 
	returns mask -> if bit is flipped children don't needed to be retested against the plane
	bit 0 = plane 0
	bit 1 = plane 1
	bit 2 = plane 2
	bit 3 = plane 3
	bit 4 = plane 4
	bit 5 = fully inside plane 5

	Fully outside 	-	the whole box is on the wrong side of the plane (fully culled)
	Fully inside	—	the whole box is safely on the correct side of the plane, with room to spare (centreDist < projRadius, meaning even the box's outer edge doesn't cross the plane).
						bit for that plane is set to 1.
	Straddling		—	the box crosses the plane; part of it is inside, part might be outside. 
						bit for that plane stays 0.

	0xFFFFFFFF = fully culled don't draw
	- any other return value is partially culled / fully visible.
	- encodes which planes can skipchecks with the children
*/
UINT __cdecl TestAABBFrustum(uintptr_t frustumContext, float* centres, float* halfExtents)

{
	float centreDist;
	float projRadius;
	bool isFullyInside;
	UINT planeIndex;
	UINT insideMask;
	float* plane;

	insideMask = 0;
	plane = (float*)(frustumContext + 0x100);

	for (planeIndex = 0; planeIndex < 6; planeIndex++, plane += 4) {
		centreDist = CalculatePlaneDistance(centres, plane);
		centreDist = centreDist + plane[3];

		projRadius = CalculateProjectedRadius(halfExtents, plane);

		// is outside of plane check
		if (centreDist < -projRadius != (isnan(centreDist) || isnan(-projRadius)))
		{
			return 0xffffffff;
		}

		isFullyInside = (UINT)(centreDist < projRadius);
		insideMask |= isFullyInside << ((byte)planeIndex & 0x1f);
	}

	return insideMask;
}

int __cdecl FUN_00435d70(USHORT frustPlaneIndex)

{
	int iVar1;

	// looks like it's a failsafe check or the frustrum mask from TestAABBFrustum
	if ((USHORT)(&DAT_006b8d28)[frustPlaneIndex] == 0xffff) {
		iVar1 = 0;
	}
	else {
		iVar1 = (UINT)(USHORT)(&DAT_006b8d28)[frustPlaneIndex] * *(int*)(&DAT_006b8c98 + (UINT)frustPlaneIndex * 0x18)
			+ (&DAT_006b8cac)[(UINT)frustPlaneIndex * 6];
	}
	return iVar1;
}

// FUN_00439810
UINT __cdecl GetDebugColorByIndex(UINT param_1) {
	UINT uVar1;
	UINT local_54[20];

	local_54[0] = 0xff444444;
	local_54[1] = 0xff00ff00;
	local_54[2] = 0xff00ffff;
	local_54[3] = 0xffff0000;
	local_54[4] = 0xffff00ff;
	local_54[5] = 0xffffff00;
	local_54[6] = 0xffffffff;
	local_54[7] = 0xff000088;
	local_54[8] = 0xff008800;
	local_54[9] = 0xff008888;
	local_54[10] = 0xff0088ff;
	local_54[11] = 0xff00ff88;
	local_54[12] = 0xff880000;
	local_54[13] = 0xff880088;
	local_54[14] = 0xffcc0088;
	local_54[15] = 0xff088844;
	local_54[16] = 0xff00cc88;
	local_54[17] = 0xfff4234f;
	local_54[18] = 0xff78934e;

	if (param_1 < 0x13) {
		uVar1 = local_54[param_1];
	}
	else {
		uVar1 = param_1 * 0x382734bd | 0xff000000;
	}
	return uVar1;
}

// FUN_0044a590
void __cdecl WriteVector3(float* pOut, float pointX, int pointY, int pointZ)

{
	// write Vector3
	*param_1 = param_2;
	param_1[1] = param_3;
	param_1[2] = param_4;
	return;
}

// FUN_0044a640 and FUN_0043cfb0
void __cdecl CopyVector3(float* dst, float* src)

{
	*dst = *src;
	dst[1] = src[1];
	dst[2] = src[2];
	return;
}

// argb is the colour
// does edge calculation
// FUN_0044a5b0
void __cdecl EnqueueEdge(LargeRenderContext* pRenderContext, float* start, float* end, int argb)

{
	RenderCommand* command;

	// is queue full
	// this probably draws to the backbuffer if the queue is full
	// then clears it to make room for more
	if (pRenderContext->queuedObjectCount == 0x100) {
		FUN_0044a670(pRenderContext);
	}

	command = &pRenderContext->commands[pRenderContext->queuedObjectCount];
	pRenderContext->queuedObjectCount++;
	CopyVector3(command->start, start);
	CopyVector3(command->end, end);
	command->flags = argb;
	return;
}


void __cdecl FUN_0044a670(LargeRenderContext* pRenderContext)

{
	RendererInstance* rendererInstance;
	uint32_t flag;
	int success;
	int queuedObjectCount;
	RenderCommand* command;

	rendererInstance = pRenderContext->pRendererInstance;
	command = pRenderContext->commands;
	queuedObjectCount = pRenderContext->queuedObjectCount;
	if (queuedObjectCount != 0) {
		success = PreparePrimitiveBatch(rendererInstance, 1, queuedObjectCount);
		if (success != 0) {
			while (queuedObjectCount != 0) {
				flag = command->flags;
				StreamVertex(rendererInstance, command->start, flag);
				StreamVertex(rendererInstance, command->end, flag);

				// next command
				command = command + 7;

				// decrement queued obj
				queuedObjectCount = queuedObjectCount - 1;
			}
			// does the render pass and that
			FUN_004b5140(rendererInstance);
			pRenderContext->queuedObjectCount = 0;
		}
	}
	return;
}

// Enqueues AABB to the rendererCOntext command queue with the given ARGB
// FUN_0044a1c0
void __cdecl EnqueueAABB(float* pBoundingBox, LargeRenderContext *pRenderContext, int argb)

{
	float centreZ;
	float halfExtentX;

	// different vector 3's / points
	float point_1[3];
	float point_2[3];
	float point_3[3];
	float point_4[3];
	float point_5[3];
	float point_6[3];
	float point_7[3];
	float point_8[3];
	float halfExtentY;
	float centreY;
	float halfExtentZ;
	float centreX;

	centreX = *pBoundingBox;
	centreY = pBoundingBox[1];
	centreZ = pBoundingBox[2];
	halfExtentX = pBoundingBox[3];
	halfExtentY = pBoundingBox[4];
	halfExtentZ = pBoundingBox[5];
	// the following points are just to visualise
	// store the -/-/- point (-1,-1,-1)
	WriteVector3(point_1, centreX - halfExtentX, centreY - halfExtentY, centreZ - halfExtentZ);
	// +/-/- point (1,-1,-1)
	WriteVector3(point_2, centreX + halfExtentX, centreY - halfExtentY, centreZ - halfExtentZ);
	// -/+/- point (-1,1,-1)
	WriteVector3(point_3, centreX - halfExtentX, centreY + halfExtentY, centreZ - halfExtentZ);
	// +/+/- point (1,1,-1)
	WriteVector3(point_4, centreX + halfExtentX, centreY + halfExtentY, centreZ - halfExtentZ);
	// -/-/+ point (-1,-1,1)
	WriteVector3(point_5, centreX - halfExtentX, centreY - halfExtentY, centreZ + halfExtentZ);
	// +/-/+ point (1,-1,1)
	WriteVector3(point_6, centreX + halfExtentX, centreY - halfExtentY, centreZ + halfExtentZ);
	// -/+/+ point (-1,1,1)
	WriteVector3(point_7, centreX - halfExtentX, centreY + halfExtentY, centreZ + halfExtentZ);
	// +/+/+ point (1,1,1)
	WriteVector3(point_8, centreX + halfExtentX, centreY + halfExtentY, centreZ + halfExtentZ);

	// load edge into RenderContext command queue
	EnqueueEdge(pRenderContext, point_1, point_2, argb);
	EnqueueEdge(pRenderContext, point_2, point_4, argb);
	EnqueueEdge(pRenderContext, point_4, point_3, argb);
	EnqueueEdge(pRenderContext, point_3, point_1, argb);
	EnqueueEdge(pRenderContext, point_5, point_6, argb);
	EnqueueEdge(pRenderContext, point_6, point_8, argb);
	EnqueueEdge(pRenderContext, point_8, point_7, argb);
	EnqueueEdge(pRenderContext, point_7, point_5, argb);
	EnqueueEdge(pRenderContext, point_1, point_5, argb);
	EnqueueEdge(pRenderContext, point_2, point_6, argb);
	EnqueueEdge(pRenderContext, point_3, point_7, argb);
	EnqueueEdge(pRenderContext, point_4, point_8, argb);
	return;
}

int __cdecl FUN_00435d70(USHORT param_1)

{
	int iVar1;

	if ((USHORT)(&DAT_006b8d28)[param_1] == 0xffff) {
		iVar1 = 0;
	}
	else {
		iVar1 = (UINT)(USHORT)(&DAT_006b8d28)[param_1] * *(int*)(&DAT_006b8c98 + (UINT)param_1 * 0x18)
			+ (&DAT_006b8cac)[(UINT)param_1 * 6];
	}
	return iVar1;
}

// FUN_0043de50 and FUN_00439950 and FUN_0043f2b0
void __cdecl FUN_00439950(LargeRenderContext* pRenderContext)

{
	RendererInstance* pRenderInstance;
	uint32_t uVar1;
	int success;
	OtherRenderCommand* command;
	UINT queuedObjectCount;

	pRenderInstance = pRenderContext->pRendererInstance;

	command = &pRenderContext->secondaryCommands[0];
	queuedObjectCount = pRenderContext->secondaryCommandObjCount;
	if (queuedObjectCount != 0) {
		// set depth bias, shift geometry forward slightly
		// probably to prevent flickering
		FUN_004b17d0(pRenderInstance, 2, 0xb727c5ac);
		success = PreparePrimitiveBatch(pRenderInstance, 3, queuedObjectCount);
		if (success != 0) {
			while (queuedObjectCount != 0) {
				uVar1 = command->flags;
				// write things to alloc result buffer
				FUN_004b41e0(pRenderInstance, &command->v1, &command->v2, &command->v3, uVar1, uVar1, uVar1);
				command++;
				queuedObjectCount--;
			}
			// does the render pass and that
			FUN_004b5140(pRenderInstance);
			pRenderContext->secondaryCommandObjCount = 0;
		}
		// change depthbias back to 0
		FUN_004b17d0(pRenderInstance, 2, 0);
	}
	return;
}


// FUN_004b17d0
void __cdecl FUN_004b17d0(RendererInstance* pRendererInstance, int mode, DWORD newValue)

{
	IDirect3DDevice9* d3dDevice;

	d3dDevice = pRendererInstance->GetDevice();
	if (mode == 2) {
		// 0xc3
		d3dDevice->SetRenderState(D3DRS_DEPTHBIAS, newValue);
	}
	else if (mode == 9) {
		d3dDevice->SetRenderState(D3DRS_ALPHAREF, newValue);
	}
	return;
}

int __cdecl FUN_00435dd0(int param_1)

{
	int iVar1;

	if (*(USHORT*)(param_1 + 0x12) == 0xffff) {
		iVar1 = 0;
	}
	else {
		iVar1 = (UINT) * (USHORT*)(param_1 + 0x12) *
			*(int*)(&DAT_006b8c98 + (UINT) * (USHORT*)(param_1 + 0x10) * 0x18) +
			(&DAT_006b8cac)[(UINT) * (USHORT*)(param_1 + 0x10) * 6];
	}
	return iVar1;
}

// streams the vertices of queued objects process effects, do the effect pass etc.
void __cdecl FUN_00439a30(LargeRenderContext* pRenderContext)

{
	RendererInstance* pRendererInstance;
	uint32_t flag;
	int success;
	UINT queuedObjectCount;
	RenderCommand* command;

	pRendererInstance = pRenderContext->pRendererInstance;
	command = pRenderContext->commands;
	queuedObjectCount = pRenderContext->queuedObjectCount;
	if (queuedObjectCount != 0) {
		success = PreparePrimitiveBatch(pRendererInstance, 1, queuedObjectCount);
		if (success != 0) {
			while (queuedObjectCount != 0) {
				flag = command->flags;
				StreamVertex(pRendererInstance, command->start, flag);
				StreamVertex(pRendererInstance, command->end, flag);
				command++;
				queuedObjectCount--;
			}
			// process effects, draw, event pass etc.
			FUN_004b5140(pRendererInstance);
			pRenderContext->queuedObjectCount = 0;
		}
	}
	return;
}

// FUN_004bbf60
float CalculatePlaneDistance(float* centres, float* param_2) {
	return (centres[0] * param_2[0]) +
		(centres[1] * param_2[1]) +
		(centres[2] * param_2[2]);
}

// FUN_004bc5c0
float __cdecl CalculateProjectedRadius(float* halfExtents, float* planeNormal)

{
	float halfExtY;
	float halfExtZ;
	float normY;
	float normZ;
	double dVar5;
	double dVar6;
	double dVar7;

	// y and z half extents (radii from centre to respective corner)
	halfExtY = halfExtents[1];
	halfExtZ = halfExtents[2];

	// y and z norm
	normY = planeNormal[1];
	normZ = planeNormal[2];


	dVar5 = FloatToAbsDouble(*halfExtents * *planeNormal);
	dVar6 = FloatToAbsDouble(halfExtY * normY);
	dVar7 = FloatToAbsDouble(halfExtZ * normZ);
	return (float)(dVar7 +dVar6 + dVar5);
}

// FUN_004bbdf0
double __cdecl FloatToAbsDouble(float num)
{
	return std::abs((double)(num));
}

// these are not all the methods, just the ones relevent to the debug
// slash rendering pipeline in this proj
#pragma region Shape/RenderObject methods
// Spheres
// FUN_0043ec70

void __cdecl FUN_0043ec70(int* this, LargeRendererContext* pRendererContext, uint argb)

{
	uint uVar1;
	void* extraout_ECX;
	void* extraout_ECX_00;
	void* pvVar2;
	void* extraout_ECX_01;
	void* extraout_ECX_02;
	float10 fVar3;
	float10 extraout_ST0;
	float fVar4;
	float fVar5;
	float10* in_stack_fffffb40;
	undefined local_4ac[12];
	float local_4a0[3];
	float local_494[3];
	float* local_488;
	float* local_484;
	float* local_480;
	float* local_47c;
	float afStack_478[245];
	float local_a4[3];
	float local_98;
	float local_94;
	float local_90;
	float local_8c;
	uint local_88;
	undefined4 uStack_84;
	uint local_80;
	float local_7c;
	float local_78;
	float local_74;
	void* local_70;
	undefined4 uStack_6c;
	void* local_64;
	PhysicsObject local_60;
	float local_20;
	int local_1c;
	int local_18;
	undefined4* local_14;

	local_14 = (undefined4*)this[3];
	local_18 = this[2];
	local_1c = this[1];
	local_20 = *(float*)(local_18 + 0xc);
	if ((local_1c == 0) || ((*(ushort*)(local_1c + 0x10) & 1) == 0)) {
		FUN_0045c110(&local_60.field0_0x0);
		FUN_0043e8d0(&local_60.x, local_14);
	}
	else {
		CopyTransform(&local_60, *(PhysicsObject**)(local_1c + 4));
		FUN_0045d5b0((float*)&local_60.x, (float*)(local_14 + 3), *(float**)(this[1] + 4));
	}
	for (local_64 = (void*)0x0; local_64 < (void*)0x9; local_64 = (void*)((int)local_64 + 1)) {
		local_70 = local_64;
		uStack_6c = 0;
		local_74 = (float)ZEXT48(local_64) * 0.3926991 + -3.141593;
		fVar3 = FUN_0043f0c0(local_64, local_74);
		local_78 = (float)fVar3;
		fVar3 = FUN_0043f0e0(extraout_ECX, local_74);
		local_7c = (float)fVar3;
		pvVar2 = extraout_ECX_00;
		for (local_80 = 0; local_80 < 9; local_80 = local_80 + 1) {
			local_88 = local_80;
			uStack_84 = 0;
			local_8c = (float)(ulonglong)local_80 * 0.7853982 + -3.141593;
			fVar3 = FUN_0043f0e0(pvVar2, local_8c);
			fVar5 = (float)(fVar3 * (float10)local_7c * (float10)local_20);
			fVar4 = local_78 * local_20;
			local_94 = fVar4;
			local_90 = fVar5;
			fVar3 = FUN_0043f0c0(extraout_ECX_01, local_8c);
			local_98 = (float)(fVar3 * (float10)local_7c * (float10)local_20);
			FUN_0043ec50(local_a4, local_98, fVar4, fVar5);
			FUN_0045d5b0(afStack_478 + (int)local_64 * 0x1b + local_80 * 3, local_a4, (float*)&local_60);
			pvVar2 = extraout_ECX_02;
		}
	}
	for (local_64 = (void*)0x0; local_64 < 8; local_64 = (void*)((int)local_64 + 1)) {
		local_47c = afStack_478 + (int)local_64 * 0x1b;
		local_480 = afStack_478 + ((int)local_64 + 1U) * 0x1b;
		for (local_80 = 0; local_80 < 8; local_80 = local_80 + 1) {
			local_484 = afStack_478 + (int)local_64 * 0x1b + (local_80 + 1) * 3;
			local_488 = afStack_478 + ((int)local_64 + 1U) * 0x1b + (local_80 + 1) * 3;
			if (local_64 == (void*)0x0) {
				FUN_0043e5b0(local_494, local_488, local_480);
				FUN_0043e5b0(local_4a0, local_47c, local_480);
			}
			else {
				FUN_0043e5b0(local_494, local_484, local_47c);
				FUN_0043e5b0(local_4a0, local_47c, local_480);
			}
			FUN_0043f100((float*)local_4ac, local_494, local_4a0);
			FUN_0043e780((float10*)local_4ac, (float*)local_4ac, in_stack_fffffb40);
			FUN_0043f090((float10*)(((float)local_4ac._4_4_ + 1.0) * 0.5 + 0.2), 1.0,
				(float)in_stack_fffffb40);
			uVar1 = FUN_0043f180(argb, (float10*)(float)extraout_ST0);
			uVar1 = argb & 0xff000000 | uVar1 & 0xffffff;
			EnqueueTriangle(pRendererContext, local_47c, local_480, local_488, uVar1);
			EnqueueTrinagle(pRendererContext, local_47c, local_488, local_484, uVar1);
			local_47c = local_484;
			local_480 = local_488;
		}
	}
	return;
}



// OBB
// FUN_0043d8e0
// param1 is this
void __cdecl FUN_0043d8e0(void* param_1, LargeRenderContext* pRenderContext, UINT argb)

{
	OBBVertexBuffer outBuffer;

	GetOBBVertices(param_1, &outBuffer);
	EnqueueOBBQuad(&outBuffer, pRenderContext, argb, 0, 2, 3, 1);
	EnqueueOBBQuad(&outBuffer, pRenderContext, argb, 1, 3, 7, 5);
	EnqueueOBBQuad(&outBuffer, pRenderContext, argb, 5, 7, 6, 4);
	EnqueueOBBQuad(&outBuffer, pRenderContext, argb, 4, 6, 2, 0);
	EnqueueOBBQuad(&outBuffer, pRenderContext, argb, 2, 6, 7, 3);
	EnqueueOBBQuad(&outBuffer, pRenderContext, argb, 1, 5, 4, 0);
	return;
}

// param1 = this
// this is a stupid ass function
// member of some PhysicsOBBNode class
// FUN_0043d040
void __cdecl GetOBBVertices(int param_1, OBBVertexBuffer* outBuffer)

{
	float extentX;
	Vector3 temp1;
	Vector3 temp2;
	Vector3 scaledX;
	Vector3 scaledY;
	Vector3 scaledZ;
	Vector3 centre;
	int innerData;
	float* matrix;

	matrix = *(float**)(param_1 + 0xc);
	innerData = *(int*)(param_1 + 8);

	extentX = *(float*)(innerData + 0xc);
	// the following aren't actually declared here
	// but im doing it for readability
	// know that if using for a decomp
	float extentY = *(float*)(innerData + 0x10);
	float extentZ = *(float*)(innerData + 0x14);
	
	outBuffer->header.dataPtr = outBuffer->vertices;
	outBuffer->header.vertexCount = 8;


	scaledX.x = *matrix * extentX;
	scaledX.y = matrix[1] * extentX;
	scaledX.z = matrix[2] * extentX;
	scaledY.x = matrix[4] * extentY;
	scaledY.y = matrix[5] * extentY;
	scaledY.z = matrix[6] * extentY;
	scaledZ.x = matrix[8] * extentZ;
	scaledZ.y = matrix[9] * extentZ;
	scaledZ.z = matrix[10] * extentZ;
	centre.x = matrix[0xc];
	centre.y = matrix[0xd];
	centre.z = matrix[0xe];

	VectorSubtraction3D(&temp2, &centre, &scaledZ);
	VectorSubtraction3D(&temp1, &temp2, &scaledX);

	VectorSubtraction3D(&outBuffer->vertices[0], &temp1, &scaledY);
	VectorAddition3D(&outBuffer->vertices[2], &temp1, &scaledY);

	VectorAddition3D(&temp1, &temp2, &scaledX);
	VectorSubtraction3D(&outBuffer->vertices[1], &temp1, &scaledY);
	VectorAddition3D(&outBuffer->vertices[3], &temp1, &scaledY);

	VectorAddition3D(&temp2, &centre, &scaledZ);
	VectorSubtraction3D(&temp1, &temp2, &scaledX);

	VectorSubtraction3D(&outBuffer->vertices[4], &temp1, &scaledY);
	VectorAddition3D(&outBuffer->vertices[6], &temp1, &scaledY);

	VectorAddition3D(&temp1, &temp2, &scaledX);
	VectorSubtraction3D(&outBuffer->vertices[5], &temp1, &scaledY);
	VectorAddition3D(&outBuffer->vertices[7], &temp1, &scaledY);
	return;
}

// FUN_0043d2e0
void __cdecl VectorSubtraction3D(Vector3* vOut, Vector3* vector_1, Vector3* vector_2)

{
	vOut->x = vector_1->x - vector_2->x;
	vOut->y = vector_1->y - vector_2->y;
	vOut->z = vector_1->z - vector_2->z;
	return;
}

// FUN_0043d270
void __cdecl VectorAddition3D(Vector3* vOut, Vector3* vector_1, Vector3* vector_2)

{
	vOut->x = vector_1->x + vector_2->x;
	vOut->y = vector_1->y + vector_2->y;
	vOut->z = vector_1->z + vector_2->z;
	return;
}

// FUN_0043d9c0
void __cdecl
EnqueueOBBQuad(OBBVertexBuffer* param_1, LargeRenderContext* pRenderContext, UINT argb, int param_4,
	int param_5, int param_6, int param_7)

{
	UINT shadedColour;
	Vector3 edge2;
	Vector3 edge1;
	Vector3 normal;
	float lightingFactor;


	VectorSubtraction3D(&edge1, &(param_1->header).dataPtr[param_5],
		&(param_1->header).dataPtr[param_4]);
	VectorSubtraction3D(&edge2, &(param_1->header).dataPtr[param_6],
		&(param_1->header).dataPtr[param_4]);
	VectorCrossProduct3D(&normal, &edge1, &edge2);
	NormaliseVector3D(&normal, &normal);

	lightingFactor = (normal.y + 1.0) * 0.5 + 0.2;

	shadedColour = ApplyLightingToColour(argb, lightingFactor);
	
	// enqueues the quad
	EnqueueTriangle(pRenderContext, &(param_1->header).dataPtr[param_4], &(param_1->header).dataPtr[param_5],
		&(param_1->header).dataPtr[param_6], shadedColour);
	EnqueueTriangle(pRenderContext, &(param_1->header).dataPtr[param_4], &(param_1->header).dataPtr[param_6],
		&(param_1->header).dataPtr[param_7], shadedColour);
	return;
}

// FUN_0043dbb0
Vector3 NormaliseVector3D(Vector3* out, const Vector3* in)
{
	// FUN_0043dc60 and FUN_0055d5b4 collapse into this but they use
	// lower level stuff that I can't be bothered with
	float length = std::sqrtf(in->x * in->x + in->y * in->y + in->z * in->z);

	if (length <= 0.0f) {
		out->x = 0.0f;
		out->y = 0.0f;
		out->z = 0.0f;
	}
	else {
		float invLength = 1.0f / length;
		out->x = in->x * invLength;
		out->y = in->y * invLength;
		out->z = in->z * invLength;
	}
}

// FUN_0043db30
void __cdecl VectorCrossProduct3D(Vector3* param_1, Vector3* param_2, Vector3* param_3)

{
	float fVar1;
	float fVar2;
	float fVar3;
	float fVar4;
	float fVar5;
	float fVar6;

	fVar1 = param_2->x;
	fVar2 = param_2->y;
	fVar3 = param_2->z;
	fVar4 = param_3->x;
	fVar5 = param_3->y;
	fVar6 = param_3->z;
	param_1->x = fVar2 * fVar6 - fVar3 * fVar5;
	param_1->y = fVar3 * fVar4 - fVar1 * fVar6;
	param_1->z = fVar1 * fVar5 - fVar2 * fVar4;
	return;
}
// FUN_0043dc80
UINT ApplyLightingToColour(UINT argb, float intensity)
{
	// Clamp the lighting factor to valid normalised bounds [0.0, 1.0]
	// replaces the bs helpers like FUN_0043dd50 FUN_0043dd80 FUN_0043db00
	// that I will not be decompiling
	float clampedIntensity = std::clamp(intensity, 0.0f, 1.0f);

	// Extract individual ARGB channels
	UINT alpha = (argb >> 24) & 0xFF;
	UINT red = (UINT)(((argb >> 16) & 0xFF) * clampedIntensity);
	UINT green = (UINT)(((argb >> 8) & 0xFF) * clampedIntensity);
	UINT blue = (UINT)((argb & 0xFF) * clampedIntensity);

	// Reassemble and return the modulated color while preserving original alpha
	return (alpha << 24) | (red << 16) | (green << 8) | blue;
}

// FUN_0043ddb0 and FUN_0043f2b0
void EnqueueTriangle(LargeRenderContext* pRenderContext, Vector3* pPoint1, Vector3* pPoint2, Vector3* pPoint3, UINT argb)

{
	OtherRenderCommand* command;

	// this mirrors pRenderContext->queuedObjectCount in other fns
	if (pRenderContext->secondaryCommandObjCount == 0x100) {
		FUN_00439950(pRenderContext);
	}
	command = &pRenderContext->secondaryCommands[pRenderContext->secondaryCommandObjCount];
	pRenderContext->secondaryCommandObjCount++;

	// copies the params to the command
	// FUN_0043cfb0
	CopyVector3((float*)&command->v1, (float*)pPoint1);
	CopyVector3((float*)&command->v2, (float*)pPoint2);
	CopyVector3((float*)&command->v3, (float*)pPoint3);
	command->flags = argb;
	return;
}


#pragma endregion Shape/RenderObject methods


