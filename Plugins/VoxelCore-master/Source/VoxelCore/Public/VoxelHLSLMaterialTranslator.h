﻿// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#if WITH_EDITOR
#include "Materials/HLSLMaterialTranslator.h"
#endif

#if WITH_EDITOR
struct FVoxelHLSLMaterialTranslator : FHLSLMaterialTranslator
{
	using FHLSLMaterialTranslator::ShaderFrequency;
	using FHLSLMaterialTranslator::MaterialProperty;
	using FHLSLMaterialTranslator::FunctionStacks;
	using FHLSLMaterialTranslator::CurrentScopeChunks;
	using FHLSLMaterialTranslator::CurrentScopeID;
	using FHLSLMaterialTranslator::VTStackHash;
	using FHLSLMaterialTranslator::CustomExpressions;
	using FHLSLMaterialTranslator::AddCodeChunk;
	using FHLSLMaterialTranslator::GetParameterCode;
	using FHLSLMaterialTranslator::CreateSymbolName;
	using FHLSLMaterialTranslator::HLSLTypeString;
	using FHLSLMaterialTranslator::HLSLTypeStringDeriv;
	using FHLSLMaterialTranslator::GetParameterCodeRaw;
};
#endif