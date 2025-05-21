#include "names_converter.h"

std::string convert_port_name(const std::string& node_name, const std::string& port_name)
{
	if (shader_names_map.contains(node_name))
	{
		std::unordered_map<std::string, std::string> node_map = shader_names_map[node_name];
		if (node_map.contains(port_name))
		{
			return node_map[port_name];
		}
		else
		{
			return "";
		}
	}
	else
	{
		return "";
	}
}

std::string convert_port_name(const XSI::CString& node_name, const XSI::CString& port_name)
{
	std::string node_name_str = std::string(node_name.GetAsciiString());
	std::string port_name_str = std::string(port_name.GetAsciiString());
	return convert_port_name(node_name_str, port_name_str);
}

ccl::ClosureType get_distribution(const XSI::CString& distribution, DistributionModes mode)
{
	if (mode == DistributionModes_Anisotropic)
	{//anisotropic
		if (distribution == "Beckmann") { return ccl::ClosureType::CLOSURE_BSDF_MICROFACET_BECKMANN_ID; }
		else if (distribution == "GGX") { return ccl::ClosureType::CLOSURE_BSDF_MICROFACET_GGX_ID; }
		else if (distribution == "Multiscatter GGX") { return ccl::ClosureType::CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID; }
		else { return ccl::ClosureType::CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID; }
	}
	else if (mode == DistributionModes_Toon)
	{//toon
		if (distribution == "Diffuse") { return ccl::ClosureType::CLOSURE_BSDF_DIFFUSE_TOON_ID; }
		else { return ccl::ClosureType::CLOSURE_BSDF_GLOSSY_TOON_ID; }
	}
	else if (mode == DistributionModes_Glossy)
	{//glossy
		if (distribution == "Beckmann") { return ccl::ClosureType::CLOSURE_BSDF_MICROFACET_BECKMANN_ID; }
		else if (distribution == "GGX") { return ccl::ClosureType::CLOSURE_BSDF_MICROFACET_GGX_ID; }
		else if (distribution == "Multiscatter GGX") { return ccl::ClosureType::CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID; }
		else { return ccl::ClosureType::CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID; }
	}
	else if (mode == DistributionModes_Glass)
	{//glass
		if (distribution == "Beckmann") { return ccl::ClosureType::CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID; }
		else if (distribution == "Multiscatter GGX") { return ccl::ClosureType::CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID; }
		else { return ccl::ClosureType::CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID; }
	}
	else if (mode == DistributionModes_Refraction)
	{//refraction
		if (distribution == "Beckmann") { return ccl::ClosureType::CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID; }
		else { return ccl::ClosureType::CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID; }
	}
	else if (mode == DistributionModes_Hair)
	{//hair
		if (distribution == "Reflection") { return ccl::ClosureType::CLOSURE_BSDF_HAIR_REFLECTION_ID; }
		else { return ccl::ClosureType::CLOSURE_BSDF_HAIR_TRANSMISSION_ID; }
	}
	else if (mode == DistributionModes_Principle)
	{//Principle
		if (distribution == "GGX") { return ccl::ClosureType::CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID; }
		else { return ccl::ClosureType::CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID; }
	}
	else
	{
		return ccl::ClosureType::CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID;
	}
}

ccl::ClosureType get_subsurface_method(const XSI::CString& method_string)
{
	if (method_string == "burley") { return ccl::ClosureType::CLOSURE_BSSRDF_BURLEY_ID; }
	else if (method_string == "random_walk_fixed") { return ccl::ClosureType::CLOSURE_BSSRDF_RANDOM_WALK_SKIN_ID; }
	else { return ccl::ClosureType::CLOSURE_BSSRDF_RANDOM_WALK_ID; }
}

ccl::NodePrincipledHairParametrization principled_hair_parametrization(const XSI::CString& key)
{
	if (key == "direct_coloring") { return ccl::NodePrincipledHairParametrization::NODE_PRINCIPLED_HAIR_REFLECTANCE; }
	else if (key == "melanin_concentration") { return ccl::NodePrincipledHairParametrization::NODE_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION; }
	else { return ccl::NodePrincipledHairParametrization::NODE_PRINCIPLED_HAIR_DIRECT_ABSORPTION; }
}

ccl::NodePrincipledHairModel principled_hair_model(const XSI::CString& key)
{
	if (key == "chiang") { return ccl::NodePrincipledHairModel::NODE_PRINCIPLED_HAIR_CHIANG; }
	else { return ccl::NodePrincipledHairModel::NODE_PRINCIPLED_HAIR_HUANG; }
}

ccl::ClosureType sheen_distribution(const XSI::CString& key)
{
	if (key == "ashikhmin") { return ccl::ClosureType::CLOSURE_BSDF_ASHIKHMIN_VELVET_ID; }
	else { return ccl::ClosureType::CLOSURE_BSDF_SHEEN_ID; }
}

int get_dimensions_type(const XSI::CString& type)
{
	if (type == "1d") { return 1; }
	else if (type == "2d") { return 2; }
	else if (type == "3d") { return 3; }
	else { return 4; }
}

ccl::NodeGaborType get_gabor_type(const XSI::CString &type)
{
	if (type == "2d") { return ccl::NodeGaborType::NODE_GABOR_TYPE_2D; }
	else { return ccl::NodeGaborType::NODE_GABOR_TYPE_3D; }
}

ccl::NodeGradientType get_gradient_type(const XSI::CString& gradient)
{
	if (gradient == "Linear") { return ccl::NodeGradientType::NODE_BLEND_LINEAR; }
	else if (gradient == "Quadratic") { return ccl::NodeGradientType::NODE_BLEND_QUADRATIC; }
	else if (gradient == "Easing") { return ccl::NodeGradientType::NODE_BLEND_EASING; }
	else if (gradient == "Diagonal") { return ccl::NodeGradientType::NODE_BLEND_DIAGONAL; }
	else if (gradient == "Radial") { return ccl::NodeGradientType::NODE_BLEND_RADIAL; }
	else if (gradient == "Quadratic Sphere") { return ccl::NodeGradientType::NODE_BLEND_QUADRATIC_SPHERE; }
	else { return ccl::NodeGradientType::NODE_BLEND_SPHERICAL; }
}

ccl::NodeVoronoiDistanceMetric voronoi_distance(const XSI::CString& key)
{
	if (key == "distance") { return ccl::NodeVoronoiDistanceMetric::NODE_VORONOI_EUCLIDEAN; }
	else if (key == "manhattan") { return ccl::NodeVoronoiDistanceMetric::NODE_VORONOI_MANHATTAN; }
	else if (key == "chebychev") { return ccl::NodeVoronoiDistanceMetric::NODE_VORONOI_CHEBYCHEV; }
	else { return ccl::NodeVoronoiDistanceMetric::NODE_VORONOI_MINKOWSKI; }
}

ccl::NodeVoronoiFeature voronoi_feature(const XSI::CString& key)
{
	if (key == "f1") { return ccl::NodeVoronoiFeature::NODE_VORONOI_F1; }
	else if (key == "f2") { return ccl::NodeVoronoiFeature::NODE_VORONOI_F2; }
	else if (key == "smooth_f1") { return ccl::NodeVoronoiFeature::NODE_VORONOI_SMOOTH_F1; }
	else if (key == "distance_to_edge") { return ccl::NodeVoronoiFeature::NODE_VORONOI_DISTANCE_TO_EDGE; }
	else { return ccl::NodeVoronoiFeature::NODE_VORONOI_N_SPHERE_RADIUS; }
}

ccl::NodeWaveProfile get_wave_profile(const XSI::CString& wave)
{
	if (wave == "Sine") { return ccl::NodeWaveProfile::NODE_WAVE_PROFILE_SIN; }
	else if (wave == "Triangle") { return ccl::NodeWaveProfile::NODE_WAVE_PROFILE_TRI; }
	else { return ccl::NodeWaveProfile::NODE_WAVE_PROFILE_SAW; }
}

ccl::NodeWaveBandsDirection get_wave_bands_direction(const XSI::CString& direction)
{
	if (direction == "x") { return ccl::NODE_WAVE_BANDS_DIRECTION_X; }
	else if (direction == "y") { return ccl::NODE_WAVE_BANDS_DIRECTION_Y; }
	else if (direction == "z") { return ccl::NODE_WAVE_BANDS_DIRECTION_Z; }
	else { return ccl::NODE_WAVE_BANDS_DIRECTION_DIAGONAL; }
}

ccl::NodeWaveRingsDirection get_wave_rings_direction(const XSI::CString& direction)
{
	if (direction == "x") { return ccl::NODE_WAVE_RINGS_DIRECTION_X; }
	else if (direction == "y") { return ccl::NODE_WAVE_RINGS_DIRECTION_Y; }
	else if (direction == "z") { return ccl::NODE_WAVE_RINGS_DIRECTION_Z; }
	else { return ccl::NODE_WAVE_RINGS_DIRECTION_SPHERICAL; }
}

ccl::NodeWaveType get_wave_type(const XSI::CString& wave)
{
	if (wave == "Bands") { return ccl::NodeWaveType::NODE_WAVE_BANDS; }
	else { return ccl::NodeWaveType::NODE_WAVE_RINGS; }
}

ccl::NodeNormalMapSpace get_normal_map_space(const XSI::CString& n_map_space)
{
	if (n_map_space == "Tangent") { return ccl::NodeNormalMapSpace::NODE_NORMAL_MAP_TANGENT; }
	else if (n_map_space == "Object") { return ccl::NodeNormalMapSpace::NODE_NORMAL_MAP_OBJECT; }
	else { return ccl::NodeNormalMapSpace::NODE_NORMAL_MAP_WORLD; }
}

ccl::NodeVectorTransformType get_vector_transform_type(const XSI::CString& type)
{
	if (type == "Vector") { return ccl::NodeVectorTransformType::NODE_VECTOR_TRANSFORM_TYPE_VECTOR; }
	else if (type == "Point") { return ccl::NodeVectorTransformType::NODE_VECTOR_TRANSFORM_TYPE_POINT; }
	else { return ccl::NodeVectorTransformType::NODE_VECTOR_TRANSFORM_TYPE_NORMAL; }
}

ccl::NodeVectorTransformConvertSpace get_vector_transform_convert_space(const XSI::CString& space)
{
	if (space == "World") { return ccl::NodeVectorTransformConvertSpace::NODE_VECTOR_TRANSFORM_CONVERT_SPACE_WORLD; }
	else if (space == "Object") { return ccl::NodeVectorTransformConvertSpace::NODE_VECTOR_TRANSFORM_CONVERT_SPACE_OBJECT; }
	else { return ccl::NodeVectorTransformConvertSpace::NODE_VECTOR_TRANSFORM_CONVERT_SPACE_CAMERA; }
}

ccl::NodeVectorRotateType get_vector_rotate_type(const XSI::CString& type)
{
	if (type == "axis_angle") { return ccl::NODE_VECTOR_ROTATE_TYPE_AXIS; }
	else if (type == "x_axis") { return ccl::NODE_VECTOR_ROTATE_TYPE_AXIS_X; }
	else if (type == "y_axis") { return ccl::NODE_VECTOR_ROTATE_TYPE_AXIS_Y; }
	else if (type == "z_axis") { return ccl::NODE_VECTOR_ROTATE_TYPE_AXIS_Z; }
	else { return ccl::NODE_VECTOR_ROTATE_TYPE_EULER_XYZ; }
}

ccl::NodeTangentDirectionType get_tangent_direction_type(const XSI::CString& tangent)
{
	if (tangent == "Radial") { return ccl::NodeTangentDirectionType::NODE_TANGENT_RADIAL; }
	else { return ccl::NodeTangentDirectionType::NODE_TANGENT_UVMAP; }
}

ccl::NodeTangentAxis get_tangent_axis(const XSI::CString& axis)
{
	if (axis == "X") { return ccl::NodeTangentAxis::NODE_TANGENT_AXIS_X; }
	else if (axis == "Y") { return ccl::NodeTangentAxis::NODE_TANGENT_AXIS_Y; }
	else { return ccl::NodeTangentAxis::NODE_TANGENT_AXIS_Z; }
}

ccl::NodeMix get_mix_type(const XSI::CString& mix)
{
	if (mix == "Mix") { return ccl::NodeMix::NODE_MIX_BLEND; }
	else if (mix == "Add") { return ccl::NodeMix::NODE_MIX_ADD; }
	else if (mix == "Multiply") { return ccl::NodeMix::NODE_MIX_MUL; }
	else if (mix == "Screen") { return ccl::NodeMix::NODE_MIX_SCREEN; }
	else if (mix == "Overlay") { return ccl::NodeMix::NODE_MIX_OVERLAY; }
	else if (mix == "Subtract") { return ccl::NodeMix::NODE_MIX_SUB; }
	else if (mix == "Divide") { return ccl::NodeMix::NODE_MIX_DIV; }
	else if (mix == "Difference") { return ccl::NodeMix::NODE_MIX_DIFF; }
	else if (mix == "Darken") { return ccl::NodeMix::NODE_MIX_DARK; }
	else if (mix == "Lighten") { return ccl::NodeMix::NODE_MIX_LIGHT; }
	else if (mix == "Dodge") { return ccl::NodeMix::NODE_MIX_DODGE; }
	else if (mix == "Burn") { return ccl::NodeMix::NODE_MIX_BURN; }
	else if (mix == "Hue") { return ccl::NodeMix::NODE_MIX_HUE; }
	else if (mix == "Saturation") { return ccl::NodeMix::NODE_MIX_SAT; }
	else if (mix == "Value") { return ccl::NodeMix::NODE_MIX_VAL; }
	else if (mix == "Color") { return ccl::NodeMix::NODE_MIX_COL; }
	else if (mix == "Soft Light") { return ccl::NodeMix::NODE_MIX_SOFT; }
	else if (mix == "exclusion") { return ccl::NodeMix::NODE_MIX_EXCLUSION; }
	else { return ccl::NodeMix::NODE_MIX_LINEAR; }
}

ccl::NodeMathType get_math_type(const XSI::CString& math)
{
	if (math == "Add" || math == "add") { return ccl::NODE_MATH_ADD; }
	else if (math == "Subtract" || math == "subtract") { return ccl::NODE_MATH_SUBTRACT; }
	else if (math == "Multiply" || math == "multiply") { return ccl::NODE_MATH_MULTIPLY; }
	else if (math == "Divide" || math == "divide") { return ccl::NODE_MATH_DIVIDE; }
	else if (math == "Sine" || math == "sine") { return ccl::NODE_MATH_SINE; }
	else if (math == "Cosine" || math == "cosine") { return ccl::NODE_MATH_COSINE; }
	else if (math == "Tangent" || math == "tangent") { return ccl::NODE_MATH_TANGENT; }
	else if (math == "Arcsine" || math == "arcsine") { return ccl::NODE_MATH_ARCSINE; }
	else if (math == "Arccosine" || math == "arccosine") { return ccl::NODE_MATH_ARCCOSINE; }
	else if (math == "Arctangent" || math == "arctangent") { return ccl::NODE_MATH_ARCTANGENT; }
	else if (math == "Power" || math == "power") { return ccl::NODE_MATH_POWER; }
	else if (math == "Logarithm" || math == "logarithm") { return ccl::NODE_MATH_LOGARITHM; }
	else if (math == "Minimum" || math == "minimum") { return ccl::NODE_MATH_MINIMUM; }
	else if (math == "Maximum" || math == "maximum") { return ccl::NODE_MATH_MAXIMUM; }
	else if (math == "Round" || math == "round") { return ccl::NODE_MATH_ROUND; }
	else if (math == "Less Than" || math == "less_than") { return ccl::NODE_MATH_LESS_THAN; }
	else if (math == "Greater Than" || math == "greater_than") { return ccl::NODE_MATH_GREATER_THAN; }
	else if (math == "Modulo" || math == "modulo") { return ccl::NODE_MATH_MODULO; }
	else if (math == "arctan2") { return ccl::NODE_MATH_ARCTAN2; }
	else if (math == "floor") { return ccl::NODE_MATH_FLOOR; }
	else if (math == "ceil") { return ccl::NODE_MATH_CEIL; }
	else if (math == "fract" || math == "fraction") { return ccl::NODE_MATH_FRACTION; }
	else if (math == "sqrt") { return ccl::NODE_MATH_SQRT; }
	else if (math == "inv_sqrt") { return ccl::NODE_MATH_INV_SQRT; }
	else if (math == "sign") { return ccl::NODE_MATH_SIGN; }
	else if (math == "exponent") { return ccl::NODE_MATH_EXPONENT; }
	else if (math == "radians") { return ccl::NODE_MATH_RADIANS; }
	else if (math == "degrees") { return ccl::NODE_MATH_DEGREES; }
	else if (math == "sinh") { return ccl::NODE_MATH_SINH; }
	else if (math == "cosh") { return ccl::NODE_MATH_COSH; }
	else if (math == "tanh") { return ccl::NODE_MATH_TANH; }
	else if (math == "trunc") { return ccl::NODE_MATH_TRUNC; }
	else if (math == "snap") { return ccl::NODE_MATH_SNAP; }
	else if (math == "wrap") { return ccl::NODE_MATH_WRAP; }
	else if (math == "compare") { return ccl::NODE_MATH_COMPARE; }
	else if (math == "multiply_add") { return ccl::NODE_MATH_MULTIPLY_ADD; }
	else if (math == "pingpong") { return ccl::NODE_MATH_PINGPONG; }
	else if (math == "smooth_min") { return ccl::NODE_MATH_SMOOTH_MIN; }
	else if (math == "smooth_max") { return ccl::NODE_MATH_SMOOTH_MAX; }
	else { return ccl::NODE_MATH_ABSOLUTE; }
}

ccl::NodeVectorMathType get_vector_math(const XSI::CString& math)
{
	if (math == "add" || math == "Add") { return ccl::NODE_VECTOR_MATH_ADD; }
	else if (math == "subtract" || math == "Subtract") { return ccl::NODE_VECTOR_MATH_SUBTRACT; }
	else if (math == "multiply") { return ccl::NODE_VECTOR_MATH_MULTIPLY; }
	else if (math == "divide") { return ccl::NODE_VECTOR_MATH_DIVIDE; }
	else if (math == "cross_product" || math == "Cross Product") { return ccl::NODE_VECTOR_MATH_CROSS_PRODUCT; }
	else if (math == "project") { return ccl::NODE_VECTOR_MATH_PROJECT; }
	else if (math == "reflect") { return ccl::NODE_VECTOR_MATH_REFLECT; }
	else if (math == "refract") { return ccl::NODE_VECTOR_MATH_REFRACT; }
	else if (math == "faceforward") { return ccl::NODE_VECTOR_MATH_FACEFORWARD; }
	else if (math == "multiply_add") { return ccl::NODE_VECTOR_MATH_MULTIPLY_ADD; }
	else if (math == "dot_product" || math == "Dot Product") { return ccl::NODE_VECTOR_MATH_DOT_PRODUCT; }
	else if (math == "distance") { return ccl::NODE_VECTOR_MATH_DISTANCE; }
	else if (math == "length") { return ccl::NODE_VECTOR_MATH_LENGTH; }
	else if (math == "scale") { return ccl::NODE_VECTOR_MATH_SCALE; }
	else if (math == "normalize") { return ccl::NODE_VECTOR_MATH_NORMALIZE; }
	else if (math == "snap") { return ccl::NODE_VECTOR_MATH_SNAP; }
	else if (math == "floor") { return ccl::NODE_VECTOR_MATH_FLOOR; }
	else if (math == "ceil") { return ccl::NODE_VECTOR_MATH_CEIL; }
	else if (math == "modulo") { return ccl::NODE_VECTOR_MATH_MODULO; }
	else if (math == "fraction") { return ccl::NODE_VECTOR_MATH_FRACTION; }
	else if (math == "absolute") { return ccl::NODE_VECTOR_MATH_ABSOLUTE; }
	else if (math == "minimum") { return ccl::NODE_VECTOR_MATH_MINIMUM; }
	else if (math == "maximum") { return ccl::NODE_VECTOR_MATH_MAXIMUM; }
	else if (math == "wrap") { return ccl::NODE_VECTOR_MATH_WRAP; }
	else if (math == "sine") { return ccl::NODE_VECTOR_MATH_SINE; }
	else if (math == "cosine") { return ccl::NODE_VECTOR_MATH_COSINE; }
	else if (math == "tangent") { return ccl::NODE_VECTOR_MATH_TANGENT; }
	else { return ccl::NODE_VECTOR_MATH_NORMALIZE; }
}

ccl::NodeMapRangeType get_map_range_type(const XSI::CString& type)
{
	if (type == "linear") { return ccl::NODE_MAP_RANGE_LINEAR; }
	else if (type == "stepped") { return ccl::NODE_MAP_RANGE_STEPPED; }
	else if (type == "smooth_step") { return ccl::NODE_MAP_RANGE_SMOOTHSTEP; }
	else { return ccl::NODE_MAP_RANGE_SMOOTHERSTEP; }
}

ccl::NodeClampType get_clamp_type(const XSI::CString& type)
{
	if (type == "min_max") { return ccl::NODE_CLAMP_MINMAX; }
	else { return ccl::NODE_CLAMP_MINMAX; }
}

ccl::NodeNoiseType get_noise_type(const XSI::CString& type) {
	if (type == "multifractal") { return ccl::NODE_NOISE_MULTIFRACTAL; }
	else if (type == "fbm") { return ccl::NODE_NOISE_FBM; }
	else if (type == "hybrid_multifractal") { return ccl::NODE_NOISE_HYBRID_MULTIFRACTAL; }
	else if (type == "ridged_multifractal") { return ccl::NODE_NOISE_RIDGED_MULTIFRACTAL; }
	else if (type == "hetero_terrain") { return ccl::NODE_NOISE_HETERO_TERRAIN; }
	else { return ccl::NODE_NOISE_FBM;  }
}

ccl::ClosureType get_metallic_fresnel(const XSI::CString& type) {
	if (type == "physical_conductor") { return ccl::ClosureType::CLOSURE_BSDF_PHYSICAL_CONDUCTOR; }
	else { return ccl::ClosureType::CLOSURE_BSDF_F82_CONDUCTOR; }
}

ccl::ClosureType get_scatter_phase(const XSI::CString& type) {
	if (type == "mie") { return ccl::ClosureType::CLOSURE_VOLUME_MIE_ID; }
	else if (type == "rayleigh") { return ccl::ClosureType::CLOSURE_VOLUME_RAYLEIGH_ID; }
	else if (type == "draine") { return ccl::ClosureType::CLOSURE_VOLUME_DRAINE_ID; }
	else if (type == "fournier_forand") { return ccl::ClosureType::CLOSURE_VOLUME_FOURNIER_FORAND_ID; }
	else { return ccl::ClosureType::CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID; }
}

ccl::ShaderBump get_shader_bump(const XSI::CString &type) {
	if (type == "center") { return ccl::ShaderBump::SHADER_BUMP_CENTER; }
	else if (type == "dx") { return ccl::ShaderBump::SHADER_BUMP_DX; }
	else if (type == "dy") { return ccl::ShaderBump::SHADER_BUMP_DY; }
	else { return ccl::ShaderBump::SHADER_BUMP_NONE; }
}
