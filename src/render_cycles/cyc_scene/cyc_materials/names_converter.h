#pragma once
#include <xsi_application.h>

#include <unordered_map>
#include <string>

#include "util/array.h"
#include "scene/shader_nodes.h"

enum DistributionModes
{
	DistributionModes_Anisotropic,
	DistributionModes_Toon,
	DistributionModes_Glossy,
	DistributionModes_Glass,
	DistributionModes_Refraction,
	DistributionModes_Hair,
	DistributionModes_Principle,
	DistributionModes_Unknown
};

static std::unordered_map<std::string, std::unordered_map<std::string, std::string>> shader_names_map = {
	{
		"CyclesShadersPlugin.CyclesDiffuseBSDF.1.0", {
			{"Color", "Color"}, 
			{"Normal", "Normal"},
			{"Roughness", "Roughness"},
	
			{"outBSDF", "BSDF"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesPrincipledBSDF.1.0", {
			{"BaseColor", "Base Color"},
			{"Metallic", "Metallic"},
			{"Roughness", "Roughness"},
			{"IOR", "IOR"},
			{"Alpha", "Alpha"}, 
			{"Normal", "Normal"},
			{"SubsurfaceWeight", "Subsurface Weight"},
			{"SubsurfaceScale", "Subsurface Scale"},
			{"SubsurfaceRadius", "Subsurface Radius"},
			{"SubsurfaceIOR", "Subsurface IOR"},
			{"SubsurfaceAnisotropy", "Subsurface Anisotropy"},
			{"SpecularIORLevel", "Specular IOR Level"}, 
			{"SpecularTint", "Specular Tint"},
			{"Anisotropic", "Anisotropic"},
			{"AnisotropicRotation", "Anisotropic Rotation"},
			{"Tangent", "Tangent"},
			{"TransmissionWeight", "Transmission Weight"},
			{"SheenWeight", "Sheen Weight"}, 
			{"SheenRoughness", "Sheen Roughness"},
			{"SheenTint", "Sheen Tint"},
			{"CoatWeight", "Coat Weight"}, 
			{"CoatRoughness", "Coat Roughness"},  
			{"CoatIOR", "Coat IOR"},
			{"CoatTint", "Coat Tint"},
			{"CoatNormal", "Coat Normal"},
			{"EmissionColor", "Emission Color"},
			{"EmissionStrength", "Emission Strength"},  
	
			{"outBSDF", "BSDF"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesAnisotropicBSDF.1.0", {
			{"Color", "Color"}, {"Roughness", "Roughness"}, {"Anisotropy", "Anisotropy"}, {"Rotation", "Rotation"}, {"Normal", "Normal"}, {"Tangent", "Tangent"}, {"outBSDF", "BSDF"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesTranslucentBSDF.1.0", {
			{"Color", "Color"}, 
			{"Normal", "Normal"}, 
	
			{"outBSDF", "BSDF"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesTransparentBSDF.1.0", {
			{"Color", "Color"},
	
			{"outBSDF", "BSDF"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesVelvetBSDF.1.0", {
			{"Color", "Color"}, {"Sigma", "Sigma"}, {"Normal", "Normal"}, {"outBSDF", "BSDF"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesToonBSDF.1.0", {
			{"Color", "Color"}, 
			{"Normal", "Normal"},
			{"Size", "Size"}, 
			{"Smooth", "Smooth"}, 
	
			{"outBSDF", "BSDF"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesGlossyBSDF.1.0", {
			{"Color", "Color"}, 
			{"Normal", "Normal"},
			{"Tangent", "Tangent"},
			{"Roughness", "Roughness"}, 
			{"Anisotropy", "Anisotropy"}, 
			{"Rotation", "Rotation"}, 
	
			{"outBSDF", "BSDF"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesGlassBSDF.1.0", {
			{"Color", "Color"}, 
			{"Normal", "Normal"},
			{"Roughness", "Roughness"}, 
			{"IOR", "IOR"},
	
			{"outBSDF", "BSDF"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesRefractionBSDF.1.0", {
			{"Color", "Color"}, 
			{"Normal", "Normal"},
			{"Roughness", "Roughness"}, 
			{"IOR", "IOR"}, 
	
			{"outBSDF", "BSDF"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesHairBSDF.1.0", {
			{"Color", "Color"}, 
			{"Offset", "Offset"}, 
			{"RoughnessU", "RoughnessU"}, 
			{"RoughnessV", "RoughnessV"},
			{"Tangent", "Tangent"},
		
			{"outBSDF", "BSDF"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesEmission.1.0", {
			{"Color", "Color"},
			{"Strength", "Strength"},
			
			{"outEmission", "Emission"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesAmbientOcclusion.1.0", {
			{"Color", "Color"},
			{"Distance", "Distance"},
			{"Normal", "Normal"}, 
		
			{"outColor", "Color"},
			{"outAO", "AO"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesBackground.1.0", {
			{"Color", "Color"},
			{"Strength", "Strength"},
		
			{"outBackground", "Background"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesHoldout.1.0", {
			{"outHoldout", "Holdout"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesAbsorptionVolume.1.0", {
			{"Color", "Color"}, 
			{"Density", "Density"}, 
		
			{"outVolume", "Volume"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesScatterVolume.1.0", {
			{"Color", "Color"}, 
			{"Density", "Density"}, {
			"Anisotropy", "Anisotropy"}, 
		
			{"outVolume", "Volume"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesPrincipledVolume.1.0", {
			{"Color", "Color"}, 
			{"Density", "Density"}, 
			{"Anisotropy", "Anisotropy"}, 
			{"AbsorptionColor", "Absorption Color"}, 
			{"EmissionStrength", "Emission Strength"}, 
			{"EmissionColor", "Emission Color"}, 
			{"BlackbodyIntensity", "Blackbody Intensity"}, 
			{"BlackbodyTint", "Blackbody Tint"}, 
			{"Temperature", "Temperature"}, 
		
			{"outVolume", "Volume"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesPrincipledHairBSDF.1.0", {
			{"Color", "Color"}, 
		{"Melanin", "Melanin"},
		{"MelaninRedness", "Melanin Redness"}, 
		{"Tint", "Tint"}, 
		{"AbsorptionCoefficient", "Absorption Coefficient"}, 
		{"AspectRatio", "Aspect Ratio"},
		{"Offset", "Offset"}, 
		{"Roughness", "Roughness"}, 
		{"RadialRoughness", "Radial Roughness"}, 
		{"Coat", "Coat"}, 
		{"IOR", "IOR"},
		{"RandomRoughness", "Random Roughness"},
		{"RandomColor", "Random Color"},  
		{"Random", "Random"}, 

		{"Rlobe", "R lobe"},
		{"TTlobe", "TT lobe"},
		{"TRTlobe", "TRT lobe"},
		
		{"outBSDF", "BSDF"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesSubsurfaceScattering.1.0", {
			{"Color", "Color"}, 
			{"Normal", "Normal"},
			{"Scale", "Scale"},
			{"SSSRadius", "Radius"},
			{"IOR", "IOR"},
			{"Anisotropy", "Anisotropy"}, 
		
			{"outBSSRDF", "BSSRDF"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesImageTexture.1.0", {
			{"Vector", "Vector"}, 
			{"outColor", "Color"}, 
		
			{"outAlpha", "Alpha"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesEnvironmentTexture.1.0", {
			{"Vector", "Vector"},

			{"outColor", "Color"},
			{"outAlpha", "Alpha"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesSkyTexture.1.0", {
			{"Vector", "Vector"},
		
			{"outColor", "Color"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesNoiseTexture.1.0", {
			{"Vector", "Vector"},
			{"W", "W"},
			{"Scale", "Scale"},
			{"Detail", "Detail"}, 
			{"Roughness", "Roughness"}, 
			{"Lacunarity", "Lacunarity"}, 
			{"Offset", "Offset"}, 
			{"Gain", "Gain"}, 
			{"Distortion", "Distortion"}, 
		
			{"outFac", "Fac"},
			{"outColor", "Color"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesCheckerTexture.1.0", {
			{"Vector", "Vector"},
			{"Color1", "Color1"}, 
			{"Color2", "Color2"}, 
			{"Scale", "Scale"}, 
		
			{"outColor", "Color"}, 
			{"outFac", "Fac"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesBrickTexture.1.0", {
			{"Vector", "Vector"},
			{"Color1", "Color1"}, 
			{"Color2", "Color2"}, 
			{"Mortar", "Mortar"}, 
			{"Scale", "Scale"}, 
			{"MortarSize", "Mortar Size"}, 
			{"MortarSmooth", "Mortar Smooth"}, 
			{"Bias", "Bias"}, 
			{"BrickWidth", "Brick Width"}, 
			{"RowHeight", "Row Height"}, 
		
			{"outColor", "Color"}, 
			{"outFac", "Fac"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesGradientTexture.1.0", {
			{"Vector", "Vector"}, 
		
			{"outColor", "Color"},
			{"outFac", "Fac"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesVoronoiTexture.1.0", {
			{"Vector", "Vector"},
			{"W", "W"}, 
			{"Scale", "Scale"}, 
			{"Detail", "Detail"}, 
			{"Roughness", "Roughness"}, 
			{"Lacunarity", "Lacunarity"}, 
			{"Smoothness", "Smoothness"}, 
			{"Exponent", "Exponent"}, 
			{"Randomness", "Randomness"}, 
		
			{"outDistance", "Distance"}, 
			{"outColor", "Color"}, 
			{"outPosition", "Position"}, 
			{"outW", "W"}, 
			{"outRadius", "Radius"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesMusgraveTexture.1.0", {
			{"W", "W"}, {"Scale", "Scale"}, {"Detail", "Detail"}, {"Dimension", "Dimension"}, {"Lacunarity", "Lacunarity"}, {"Offset", "Offset"}, {"Gain", "Gain"}, {"Vector", "Vector"}, {"outFac", "Fac"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesMagicTexture.1.0", {
			{"Vector", "Vector"},
			{"Scale", "Scale"}, 
			{"Distortion", "Distortion"},  
		
			{"outColor", "Color"}, 
			{"outFac", "Fac"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesWaveTexture.1.0", {
			{"Vector", "Vector"},
			{"Scale", "Scale"}, 
			{"Distortion", "Distortion"}, 
			{"Detail", "Detail"}, 
			{"DetailScale", "Detail Scale"}, 
			{"DetailRoughness", "Detail Roughness"}, 
			{"PhaseOffset", "Phase Offset"}, 
		
			{"outColor", "Color"}, 
			{"outFac", "Fac"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesIESTexture.1.0", {
			{"Strength", "Strength"}, {"Vector", "Vector"}, {"outFac", "Fac"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesWhiteNoiseTexture.1.0", {
			{"Vector", "Vector"},
			{"W", "W"}, 
		
			{"outValue", "Value"}, 
			{"outColor", "Color"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesNormal.1.0", {
			{"Normal", "Normal"}, 
		
			{"outNormal", "Normal"},
			{"outDot", "Dot"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesBump.1.0", {
			{"Height", "Height"},
			{"Normal", "Normal"},
			{"Strength", "Strength"},
			{"Distance", "Distance"}, 
		
			{"outNormal", "Normal"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesMapping.1.0", {
			{"Vector", "Vector"}, 
			{"MapLocation", "Location"}, 
			{"MapRotation", "Rotation"}, 
			{"MapScale", "Scale"}, 
		
			{"outVector", "Vector"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesNormalMap.1.0", {
			{"Strength", "Strength"}, 
			{"Color", "Color"},
		
			{"outNormal", "Normal"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesVectorTransform.1.0", {
			{"Vector", "Vector"}, 
		
			{"outVector", "Vector"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesVectorRotate.1.0", {
			{"Vector", "Vector"}, 
			{"VectorRotation", "Rotation"},
			{"Center", "Center"}, 
			{"Axis", "Axis"},
			{"Angle", "Angle"},
		
			{"outVector", "Vector"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesVectorCurves.1.0", {
			{"Fac", "Fac"}, 
			{"Vector", "Vector"},
		
			{"outVector", "Vector"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesDisplacement.1.0", {
			{"Height", "Height"},
			{"Midlevel", "Midlevel"}, 
			{"Scale", "Scale"}, 
			{"Normal", "Normal"}, 
		
			{"outDisplacement", "Displacement"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesVectorDisplacement.1.0", {
			{"Vector", "Vector"},
			{"Midlevel", "Midlevel"}, 
			{"Scale", "Scale"}, 
		
			{"outDisplacement", "Displacement"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesGeometry.1.0", {
			{"outPosition", "Position"}, 
			{"outNormal", "Normal"}, 
			{"outTangent", "Tangent"}, 
			{"outTrueNormal", "True Normal"}, 
			{"outIncoming", "Incoming"}, 
			{"outParametric", "Parametric"}, 
			{"outBackfacing", "Backfacing"},
			{"outPointiness", "Pointiness"}, 
			{"outRandomPerIsland", "Random Per Island"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesTextureCoordinate.1.0", {
			{"outGenerated", "Generated"}, 
			{"outNormal", "Normal"},
			{"outUV", "UV"},
			{"outObject", "Object"},
			{"outCamera", "Camera"}, 
			{"outWindow", "Window"},
			{"outReflection", "Reflection"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesLightPath.1.0", {
			{"outIsCameraRay", "Is Camera Ray"},
			{"outIsShadowRay", "Is Shadow Ray"},
			{"outIsDiffuseRay", "Is Diffuse Ray"},
			{"outIsGlossyRay", "Is Glossy Ray"}, 
			{"outIsSingularRay", "Is Singular Ray"}, 
			{"outIsReflectionRay", "Is Reflection Ray"},
			{"outIsTransmissionRay", "Is Transmission Ray"}, 
			{"outIsVolumeScatterRay", "Is Volume Scatter Ray"},

			{"outRayLength", "Ray Length"}, 
			{"outRayDepth", "Ray Depth"}, 
			{"outDiffuseDepth", "Diffuse Depth"}, 
			{"outGlossyDepth", "Glossy Depth"},
			{"outTransparentDepth", "Transparent Depth"}, 
		{"outTransmissionDepth", "Transmission Depth"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesObjectInfo.1.0", {
			{"outLocation", "Location"}, 
			{"outColor", "Color"},
			{"outAlpha", "Alpha"},
			{"outObjectIndex", "Object Index"},
			{"outMaterialIndex", "Material Index"}, 
			{"outRandom", "Random"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesVolumeInfo.1.0", {
			{"outColor", "Color"},
			{"outDensity", "Density"},
			{"outFlame", "Flame"}, 
			{"putTemperature", "Temperature"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesPointInfo.1.0", {
			{"outPosition", "Position"}, 
			{"outRadius", "Radius"}, 
			{"outRandom", "Random"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesParticleInf.1.0", {
			{"outIndex", "Index"}, {"outRandom", "Random"}, {"outAge", "Age"}, {"outLifetime", "Lifetime"}, {"outLocation", "Location"}, {"outSize", "Size"}, {"outVelocity", "Velocity"}, {"outAngularVelocity", "Angular Velocity"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesHairInfo.1.0", {
			{"outIsStrand", "Is Strand"},
			{"outIntercept", "Intercept"},
			{"outLength", "Length"}, 
			{"outThickness", "Thickness"}, 
			{"outTangentNormal", "Tangent Normal"}, 
			{"outRandom", "Random"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesValue.1.0", {
			{"outValue", "Value"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesColor.1.0", {
			{"outColor", "Color"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesAttribute.1.0", {
			{"outColor", "Color"}, 
			{"outVector", "Vector"},
			{"outFac", "Fac"}, 
			{"outAlpha", "Alpha"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesOutputColorAOV.1.0", {
			{"Color", "Color"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesOutputValueAOV.1.0", {
			{"Value", "Value"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesVertexColor.1.0", {
			{"outColor", "Color"},
			{"outAlpha", "Alpha"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesBevel.1.0", {
			{"Radius", "Radius"}, 
			{"Normal", "Normal"},
		
			{"outNormal", "Normal"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesUVMap.1.0", {
			{"outUV", "UV"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesCamera.1.0", {
			{"outViewVector", "View Vector"}, 
			{"outViewZDepth", "View Z Depth"},
			{"outViewDistance", "View Distance"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesFresnel.1.0", {
			{"Normal", "Normal"},
			{"IOR", "IOR"},
		
			{"outFac", "Fac"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesLayerWeight.1.0", {
			{"Normal", "Normal"},
			{"Blend", "Blend"}, 
		
			{"outFresnel", "Fresnel"}, 
			{"outFacing", "Facing"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesWireframe.1.0", {
			{"Size", "Size"}, 
		
			{"outFac", "Fac"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesTangent.1.0", {
			{"outTangent", "Tangent"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesLightFalloff.1.0", {
			{"Strength", "Strength"}, 
			{"Smooth", "Smooth"}, 
		
			{"outQuadratic", "Quadratic"},
			{"outLinear", "Linear"}, 
			{"outConstant", "Constant"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesInvert.1.0", {
			{"Fac", "Fac"}, 
			{"Color", "Color"}, 
		
			{"outColor", "Color"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesMixRGB.1.0", {
			{"Fac", "Fac"}, 
			{"Color1", "Color1"},
			{"Color2", "Color2"},
			{"outColor", "Color"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesMixColor.1.0", {
			{"Factor", "Factor"},
			{"A", "A"},
			{"B", "B"},
			{"outResult", "Result"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesMixFloat.1.0", {
			{"Factor", "Factor"},
			{"A", "A"},
			{"B", "B"},
			{"outResult", "Result"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesMixVector.1.0", {
			{"Factor", "Factor"},
			{"A", "A"},
			{"B", "B"},
			{"outResult", "Result"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesMixVectorNonUniform.1.0", {
			{"Factor", "Factor"},
			{"A", "A"},
			{"B", "B"},
			{"outResult", "Result"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesGamma.1.0", {
			{"Color", "Color"}, 
			{"Gamma", "Gamma"}, 
		
			{"outColor", "Color"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesBrightContrast.1.0", {
			{"Color", "Color"}, 
			{"Bright", "Bright"}, 
			{"Contrast", "Contrast"}, 
		
			{"outColor", "Color"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesHSV.1.0", {
			{"Hue", "Hue"},
			{"Saturation", "Saturation"},
			{"Value", "Value"}, 
			{"Fac", "Fac"}, 
			{"Color", "Color"}, 
		
			{"outColor", "Color"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesRGBCurves.1.0", {
			{"Fac", "Fac"},
			{"Color", "Color"}, 
		
			{"outColor", "Color"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesColorCurves.1.0", {
			{"Fac", "Fac"}, {"Color", "Color"}, {"outColor", "Color"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesWavelength.1.0", {
			{"Wavelength", "Wavelength"}, 
		
			{"outColor", "Color"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesBlackbody.1.0", {
			{"Temperature", "Temperature"},
		
			{"outColor", "Color"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesMixClosure.1.0", {
			{"Fac", "Fac"},
			{"Closure1", "Closure1"},
			{"Closure2", "Closure2"}, 
		
			{"outClosure", "Closure"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesAddClosure.1.0", {
			{"Closure1", "Closure1"},
			{"Closure2", "Closure2"}, 

			{"outClosure", "Closure"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesCombineColor.1.0", {
			{"A", "Red"}, 
			{"B", "Green"}, 
			{"C", "Blue"}, 
		
			{"outColor", "Color"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesCombineRGB.1.0", {
			{"R", "R"},
			{"G", "G"},
			{"B", "B"}, 
		
			{"outImage", "Image"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesCombineHSV.1.0", {
			{"H", "H"}, 
			{"S", "S"},
			{"V", "V"},
		
			{"outColor", "Color"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesCombineXYZ.1.0", {
			{"X", "X"}, 
			{"Y", "Y"}, 
			{"Z", "Z"}, 
		
			{"outVector", "Vector"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesSeparateColor.1.0", {
			{"Color", "Color"}, 
		
			{"outA", "Red"},
			{"outB", "Green"}, 
			{"outC", "Blue"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesSeparateRGB.1.0", {
			{"Image", "Image"},
		
			{"outR", "R"}, 
			{"outG", "G"}, 
			{"outB", "B"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesSeparateHSV.1.0", {
			{"Color", "Color"}, 
		
			{"outH", "H"},
			{"outS", "S"},
			{"outV", "V"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesSeparateXYZ.1.0", {
			{"Vector", "Vector"}, 
		
			{"outX", "X"}, 
			{"outY", "Y"},
			{"outZ", "Z"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesMath.1.0", {
			{"Value1", "Value1"},
			{"Value2", "Value2"}, 
			{"Value3", "Value3"},
		
			{"outValue", "Value"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesVectorMath.1.0", {
			{"Vector1", "Vector1"}, 
			{"Vector2", "Vector2"}, 
			{"Vector3", "Vector3"},
			{"Scale", "Scale"}, 
		
			{"outValue", "Value"} ,
			{"outVector", "Vector"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesColorRamp.1.0", {
			{"Fac", "Fac"}, 
		
			{"outColor", "Color"}, 
			{"outAlpha", "Alpha"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesMapRange.1.0", {
			{"Value", "Value"},
			{"FromMin", "From Min"}, 
			{"FromMax", "From Max"}, 
			{"ToMin", "To Min"}, 
			{"ToMax", "To Max"},
			{"Steps", "Steps"}, 
		
			{"outResult", "Result"}
			}
	},
	{
		"CyclesShadersPlugin.CyclesVectorMapRange.1.0", {
			{"Vector", "Vector"}, 
			{"FromMinVector", "From_Min_FLOAT3"}, 
			{"FromMaxVector", "From_Max_FLOAT3"}, 
			{"ToMinVector", "To_Min_FLOAT3"}, 
			{"ToMaxVector", "To_Max_FLOAT3"}, 
			{"StepsVector", "Steps_FLOAT3"}, 
		
			{"outVector", "Vector"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesFloatCurve.1.0", {
			{"Fac", "Factor"}, 
			{"Value", "Value"}, 
		
			{"outValue", "Value"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesClamp.1.0", {
			{"Value", "Value"}, 
			{"Min", "Min"}, 
			{"Max", "Max"}, 
		
			{"outResult", "Result"}
		}
	},
	{
		"CyclesShadersPlugin.CyclesRGBToBW.1.0", {
			{"Color", "Color"}, 
		
			{"outVal", "Val"}
		}
	},
	{
		"Softimage.sib_scalar_to_color.1.0", {
			{"out", "Color"}  // replace by RGBRampNode
		}
	},
	{
		"Softimage.sib_vector_to_color.1.0", {
			{"out", "Image"}  // output node is CombineRGB
		}
	},
			{
		"Softimage.sib_vector_to_scalar.1.0", {
			{"out", "Value"}  // output node is Math
		}
	},
	{
		"Softimage.sib_color_to_scalar.1.0", {
			{"out", "Val"}  // replace by RGBToBW
		}
	},
	{
		"Softimage.txt2d-image-explicit.1.0", {
			{"out", "Color"}  // replace by ImageTexture
		}
	},
	{
		"Softimage.material-lambert.1.0", {
			{"out", "BSDF"}  // replace by DiffuseBSDF
		}
	},
	{
		"Softimage.material-phong.1.0", {
			{"out", "BSDF"}  // replace by PrincipledBSDF
		}
	},
	{
		"Softimage.rh_renderer.1.0", {
			{"out", "BSDF"}  // replace by HairBsdfNode
		}
	},
	{
		"GLTFShadersPlugin.MetallicRoughness.1.0", {
			{"BSDF", "BSDF"}  // replace by PrincipledBSDF
		}
	}
};

std::string convert_port_name(const std::string& node_name, const std::string& port_name);
std::string convert_port_name(const XSI::CString& node_name, const XSI::CString& port_name);

ccl::ClosureType get_distribution(const XSI::CString& distribution, DistributionModes mode);
ccl::ClosureType get_subsurface_method(const XSI::CString& method_string);
ccl::NodePrincipledHairParametrization principled_hair_parametrization(const XSI::CString& key);
int get_dimensions_type(const XSI::CString& type);
ccl::NodeGradientType get_gradient_type(const XSI::CString& gradient);
ccl::NodeVoronoiDistanceMetric voronoi_distance(const XSI::CString& key);
ccl::NodeVoronoiFeature voronoi_feature(const XSI::CString& key);
ccl::NodeMusgraveType get_musgrave_type(const XSI::CString& musgrave);
ccl::NodeWaveProfile get_wave_profile(const XSI::CString& wave);
ccl::NodeWaveBandsDirection get_wave_bands_direction(const XSI::CString& direction);
ccl::NodeWaveRingsDirection get_wave_rings_direction(const XSI::CString& direction);
ccl::NodeWaveType get_wave_type(const XSI::CString& wave);
ccl::NodeNormalMapSpace get_normal_map_space(const XSI::CString& n_map_space);
ccl::NodeVectorTransformType get_vector_transform_type(const XSI::CString& type);
ccl::NodeVectorTransformConvertSpace get_vector_transform_convert_space(const XSI::CString& space);
ccl::NodeVectorRotateType get_vector_rotate_type(const XSI::CString& type);
ccl::NodeTangentDirectionType get_tangent_direction_type(const XSI::CString& tangent);
ccl::NodeTangentAxis get_tangent_axis(const XSI::CString& axis);
ccl::NodeMix get_mix_type(const XSI::CString& mix);
ccl::NodeMathType get_math_type(const XSI::CString& math);
ccl::NodeVectorMathType get_vector_math(const XSI::CString& math);
ccl::NodeMapRangeType get_map_range_type(const XSI::CString& type);
ccl::NodeClampType get_clamp_type(const XSI::CString& type);