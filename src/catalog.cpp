#include "internal.hpp"
#include <set>

namespace foliage {
namespace {
Json field(const std::string& type, Json value, const std::string& text) { return {{"type",type},{"default",value},{"description",text}}; }
Json number(float value, float min, float max, const std::string& text) { auto f=field("number",value,text); f["min"]=min; f["max"]=max; return f; }
Json integer(int value, int min, int max, const std::string& text) { auto f=number(float(value),float(min),float(max),text); f["type"]="integer"; f["default"]=value; return f; }
Json choice(const std::string& value, Json values, const std::string& text) { auto f=field("string",value,text); f["enum"]=values; return f; }
Json ref(Json kinds, bool required=true) { return {{"type","reference"},{"required",required},{"kinds",kinds},{"description","Name of an upstream node."}}; }
Json range(Json value, float min, float max, const std::string& text) { auto f=field("pair",value,text); f["min"]=min; f["max"]=max; return f; }
Json positive(float value, const std::string& text) { return number(value,0.00001f,10000,text); }
Json seed() { return {{"type","integer"},{"min",-2147483648.0},{"max",2147483647.0},{"description","Optional node seed, independent of the root seed. Node name still identifies its random stream."}}; }
Json vec(Json value, const std::string& text) { auto f=field("vec3",value,text); f["min"]=-100000; f["max"]=100000; return f; }
Json boundedVec(Json value, float min, float max, const std::string& text) { auto f=vec(value,text); f["min"]=min; f["max"]=max; return f; }
Json profile(float min, float max, const std::string& text) { auto f=field("profile",{{0,1},{1,1}},text+" Supply 2..32 keys, strictly increasing from position 0 to 1; bounds apply to multipliers."); f["min"]=min; f["max"]=max; return f; }
Json shapeFields() {
    return {{"length",positive(1,"Path length in meters.")},{"radius",positive(0.04f,"Base radius in meters.")},
        {"taper",number(0.9f,0,0.999f,"Fraction of radius lost at the tip; tip remains nonzero.")},
        {"segments",integer(12,1,512,"Longitudinal growth steps.")},{"bend",vec({0,0,0},"Quadratic displacement at the tip in world meters.")},
        {"noise",number(0.04f,0,2,"Seeded directional variation per step.")},
        {"noise_profile",profile(0,1,"Piecewise-linear [normalized length, multiplier] keys. Fades new directional perturbations, not bends already accumulated.")},
        {"radius_profile",profile(0.001f,10,"Piecewise-linear radius multiplier over normalized length, applied after taper and flare.")},{"seed",seed()}};
}
}
Json catalog() {
    Json c=Json::object();
    auto add=[&](const std::string& name, const std::string& output, const std::string& description, Json fields) { c[name]={{"output",output},{"description",description},{"parameters",fields}}; };
    auto trunk=shapeFields(); trunk["length"]["default"]=4; trunk["radius"]["default"]=0.2;
    trunk["origin"]=vec({0,0,0},"Root position in meters."); trunk["direction"]=vec({0,1,0},"Nonzero initial direction.");
    trunk["flare"]=number(0.25f,0,5,"Additional fractional base radius, fading over flare_length.");
    trunk["flare_length"]=number(0.25f,0.001f,1,"Fraction of stem length over which the base flare fades.");
    trunk["flare_power"]=number(2,0.1f,8,"Power of the flare falloff; higher values concentrate it near the base.");
    add("trunk","skeleton","Create a stem, trunk, culm, root or stalk as a sampled growth path.",trunk);
    add("curve","skeleton","Author an explicit polyline path; tube and scatter use the same path representation.",{
        {"points",{{"type","positions"},{"required",true},{"description","2..513 distinct consecutive XYZ positions in meters."}}},
        {"radius",positive(0.04f,"Base radius.")},{"taper",number(0.9f,0,0.999f,"Fractional tip radius reduction.")},
        {"radius_profile",profile(0.001f,10,"Piecewise-linear radius multiplier over normalized arc length, applied after taper.")}});
    auto branch=shapeFields(); branch.erase("radius");
    branch["input"]=ref({"skeleton"}); branch["count"]=integer(8,0,10000,"New branches per input stem. Output contains children only.");
    branch["range"]=range({0.2,0.95},0,1,"Attachment interval along parent arc length, ordered low to high.");
    branch["angle"]=number(48,0,180,"Angle from parent tangent in degrees.");
    branch["angle_jitter"]=number(12,0,180,"Random +/- branch angle.");
    branch["azimuth"]=number(137.50776f,-360,360,"Successive azimuth increment in degrees.");
    branch["distribution"]=choice("alternate",{"alternate","opposite","whorled","random"},"Phyllotaxis pattern.");
    branch["whorl"]=integer(3,2,32,"Branches sharing a height in whorled mode.");
    branch["radius_scale"]=number(0.4f,0.001f,1,"Child base radius relative to parent radius at attachment.");
    branch["length_variation"]=number(0.25f,0,0.95f,"Random fractional +/- length variation.");
    branch["length_decay"]=number(0.4f,0,0.99f,"Shorten children nearer parent tip.");
    add("branch","skeleton","Attach another generation of paths. Chain nodes for recursive branching.",branch);
    add("roots","skeleton","Create spreading root paths from each input stem, descending to a world-Y ground plane and burying their tips. Output contains roots only; tube them and merge with the trunk.",{
        {"input",ref({"skeleton"})},{"count",integer(8,0,10000,"Roots per input stem.")},
        {"attachment",number(0.05f,0,1,"Attachment on the parent by normalized arc length. Roots start on its centerline to overlap the parent tube.")},
        {"length",positive(2,"Horizontal root reach in meters before variation.")},
        {"length_variation",number(0.25f,0,0.95f,"Seeded fractional +/- reach variation.")},
        {"angle",number(0,-36000,36000,"Starting angle around world +Y, in degrees from +X toward +Z.")},
        {"angle_jitter",number(10,0,180,"Seeded +/- azimuth variation in degrees.")},
        {"ground_height",number(0,-100000,100000,"World Y of the flat ground surface. Does not sample a terrain mesh.")},
        {"surface_at",number(0.45f,0.01f,1,"Fraction of reach by which the centerline has descended to ground level; smaller values make a steeper buttress.")},
        {"bury_depth",number(0.2f,0,10000,"Tip centerline depth below ground in meters. Choose more than tip radius to hide its cap.")},
        {"radius_scale",number(0.55f,0.001f,1,"Root base radius relative to the parent radius at attachment.")},
        {"radius_variation",number(0.2f,0,0.95f,"Seeded fractional +/- base radius variation.")},
        {"taper",number(0.98f,0,0.999f,"Fraction of base radius lost at the tip.")},
        {"radius_profile",profile(0.001f,10,"Piecewise-linear radius multiplier over normalized horizontal reach, applied after taper.")},
        {"segments",integer(32,2,512,"Samples along each root, minus one.")},
        {"noise",number(0.08f,0,10000,"Coherent sideways meander amplitude in meters. Preserves attachment and ground descent.")},
        {"noise_scale",positive(1,"Meander frequency in cycles per meter of horizontal reach.")},
        {"noise_profile",profile(0,1,"Piecewise-linear sideways noise multiplier over normalized horizontal reach; the attachment always stays fixed.")},
        {"seed",seed()}});
    auto growthProfile=profile(0,1,"Directional attraction multiplier over retained normalized arc length. Zero keeps the source tangent; one applies full strength. Use an early plateau for hanging strands.");
    growthProfile["default"]={{0,0},{1,1}};
    add("grow","skeleton","Shorten and bend each input stem from its own base, preserving retained segment lengths. Apply before attaching children.",{
        {"input",ref({"skeleton"})},{"amount",number(1,0.001f,1,"Retained fraction of arc length.")},
        {"radius_scale",positive(1,"Radius multiplier.")},{"direction",vec({0,1,0},"Nonzero tropism direction, world space.")},
        {"strength",number(0.2f,0,1,"Directional attraction, multiplied by strength_profile.")},{"strength_profile",growthProfile},
        {"interpolation",choice("linear",{"linear","spherical"},"Direction blend. Spherical turns through the angle smoothly, including opposite directions with a deterministic perpendicular bend plane. Linear retains the original normalized-vector blend.")}});
    add("prune","skeleton","Keep complete stems whose tip is inside or outside a sphere; does not cut tubes.",{
        {"input",ref({"skeleton"})},{"center",vec({0,2,0},"Selection sphere center.")},{"radius",positive(2,"Selection sphere radius.")},
        {"inside",field("boolean",true,"Keep inside the sphere; false keeps outside.")}});
    add("tube","mesh","Sweep rings over paths with transported frames, seam UVs and separate cap materials.",{
        {"input",ref({"skeleton"})},{"sides",integer(8,3,128,"Radial sides. Lower values provide cheaper geometry.")},
        {"stride",integer(1,1,512,"Use every Nth path sample, always retaining both ends, for structural LOD.")},
        {"caps",field("boolean",true,"Close both ends with separate normals and disk UVs.")},
        {"material",field("material","bark","Side material name.")},{"cap_material",field("material","cut","End material name.")},
        {"uv_scale",range({1,1},0.00001f,10000,"[circumference repeats, longitudinal repeats per meter].")},
        {"ridges",integer(0,0,128,"Longitudinal bark grooves around the circumference.")},
        {"ridge_depth",number(0.08f,0,0.8f,"Fractional radial modulation.")},
        {"ridge_profile",profile(0,1,"Piecewise-linear ridge-depth multiplier over normalized arc length. Fade basal lobes up the trunk.")},
        {"radius_noise",number(0,0,0.8f,"Seeded coherent radial surface noise as a fraction of the swept radius. Zero preserves the original surface.")},
        {"noise_scale",positive(1,"Surface-noise frequency in cycles per world meter; independent of tessellation.")},
        {"noise_octaves",integer(3,1,6,"Noise layers with doubled frequency and halved amplitude, normalized to stay within radius_noise.")},
        {"noise_profile",profile(0,1,"Piecewise-linear radial-noise multiplier over normalized arc length. Use zero at an end to remove displacement there.")},
        {"seed",seed()},
        {"node_spacing",number(0,0,10000,"Meters between bamboo-like raised rings; 0 disables.")},
        {"node_strength",number(0.15f,0,2,"Raised ring radius fraction. Requires adequate path segments.")}});
    auto proximity=ref({"mesh"},false); proximity["description"]="Optional supporting mesh. Enables distance filtering of candidate pivots after offsets.";
    add("scatter","points","Attach oriented points along paths, uniformly over triangle area, or across a ground disk.",{
        {"input",ref({"skeleton","mesh"},false)},{"count",integer(32,0,100000,"Points per stem, or total on mesh/ground.")},
        {"range",range({0.1,1},0,1,"Ordered arc-length interval for stem attachments.")},
        {"distribution",choice("alternate",{"alternate","opposite","whorled","random","tip"},"Pattern on paths; tip places all points at the tip.")},
        {"whorl",integer(3,2,32,"Members in each whorl.")},{"azimuth",number(137.50776f,-360,360,"Degrees between successive attachment groups.")},
        {"angle",number(65,0,180,"Direction angle from path tangent or surface normal.")},
        {"normal_direction",vec(nullptr,"Optional nonzero direction in graph coordinates. With mesh input, accept only interpolated surface normals within max_surface_angle of this direction. Filtering precedes offset, tilt and proximity checks.")},
        {"max_surface_angle",number(90,0,180,"With normal_direction, maximum normal angle in degrees. 90 selects the facing hemisphere; 0 selects aligned normals; 180 accepts all. Count is attempted, with no retries. This does not aim the instance; use angle:0 for local +Y along the normal.")},
        {"jitter",number(0.15f,0,1,"Random fractional position/angle variation.")},{"radius",positive(1,"Ground disk radius in meters.")},
        {"offset",number(0,-10000,10000,"Extra distance along stem radial direction or surface normal.")},
        {"proximity_mesh",proximity},
        {"max_distance",number(0.005f,0,10000,"With proximity_mesh, reject candidates farther than this many meters from its triangles. Count is an attempt limit; rejected points are not replaced.")},
        {"snap_to_mesh",field("boolean",false,"With proximity_mesh, snap accepted pivots to the nearest triangle surface, retaining orientation, scale and phase. Far candidates are still rejected. Alpha stencils are not evaluated.")},
        {"scale",range({0.8,1.2},0.00001f,10000,"Ordered instance scale range.")},{"seed",seed()}});
    add("radial","points","Place oriented points in a ring or radial spiral. Use for petals, rosettes, cones and seed heads.",{
        {"count",integer(12,0,100000,"Number of placements.")},{"center",vec({0,0,0},"Center position.")},
        {"radius",range({0.1,0.1},0,10000,"Start and end radius, may increase or decrease.")},
        {"height",range({0,0},-10000,10000,"Start and end height relative to center.")},
        {"angle",number(0,-36000,36000,"Starting azimuth in degrees.")},
        {"mode",choice("ring",{"ring","spiral"},"Ring uses evenly spaced 360 degrees; spiral uses angle_step.")},
        {"angle_step",number(137.50776f,-36000,36000,"Successive spiral azimuth increment, degrees.")},
        {"tilt",range({60,60},0,180,"Start and end angle from up; local +Z faces radially outward.")},
        {"scale",range({1,1},0.00001f,10000,"Start and end placement scale.")},
        {"jitter",number(0.04f,0,0.95f,"Random fractional radius/scale and angular variation.")},{"seed",seed()}});
    Json leaf={{"length",positive(0.25f,"Length along local +Y, base at origin.")},{"width",positive(0.12f,"Maximum full width along X.")},
        {"width_segments",integer(2,2,32,"Subdivisions across the curved surface; increase for smooth petals.")},{"segments",integer(8,2,128,"Longitudinal rows.")},{"shape",choice("oval",{"oval","lanceolate","needle","petal"},"Width profile.")},
        {"curl",number(0.15f,-4,4,"Quadratic tip bend along local Z, in multiples of length.")},
        {"fold",number(0.12f,-2,2,"Edge Z displacement in multiples of half-width.")},
        {"twist",number(0,-720,720,"Gradual rotation about local Y in degrees.")},
        {"material",field("material","leaf","Material slot.")}};
    auto widthProfile=profile(0,2,"Optional full-width envelope in multiples of width, overriding shape's width outline. End values must be zero; interior values must be positive."); widthProfile.erase("default");
    leaf["width_profile"]=widthProfile;
    leaf["lateral_bend"]=number(0,-2,2,"Quadratic lateral tip bend along local X, in multiples of length. Base stays fixed.");
    leaf["edge_wave"]=number(0,0,1,"Ripple amplitude along local Z as a fraction of half-width, strongest at the edges and zero at the base and tip.");
    leaf["edge_frequency"]=number(3,0.1f,32,"Number of edge ripple cycles over the leaf length.");
    add("leaf","mesh","Generate a tapered, curved leaf, blade or petal with midrib and normalized UVs.",leaf);
    add("card","mesh","Create rectangular atlas cards, optionally crossed. Supply alpha-cutout material for silhouettes.",{
        {"width",positive(0.3f,"Card width.")},{"height",positive(0.5f,"Card height along +Y.")},
        {"planes",integer(1,1,8,"Crossed planes around local Y.")},{"pivot",choice("base",{"base","center"},"Card anchor.")},
        {"segments",integer(1,1,128,"Longitudinal subdivisions; use at least 2 for curl or twist.")},
        {"width_segments",integer(1,1,32,"Width subdivisions; use at least 2 for fold.")},
        {"curl",number(0,-4,4,"Quadratic bend along local Z, in multiples of height; anchored at pivot Y.")},
        {"fold",number(0,-2,2,"Smooth crosswise cup: edge Z displacement in multiples of half-width.")},
        {"twist",number(0,-720,720,"Rotation about local Y in degrees per card height, zero at pivot Y.")},
        {"uv_rect",field("rect",{0,0,1,1},"Atlas [u0,v0,u1,v1], strictly increasing within 0..1.")},
        {"material",field("material","leaf","Alpha-cutout or opaque material slot.")}});
    add("ellipsoid","mesh","Create a closed bud, grain, fruit or flower center, centered at the origin.",{
        {"radii",vec({0.05,0.08,0.05},"Positive XYZ radii in meters.")},{"rings",integer(8,3,128,"Latitude subdivisions.")},
        {"sides",integer(12,3,128,"Longitude subdivisions.")},{"material",field("material","leaf","Material slot.")}});
    auto jitter=vec({0,0,0},"Seeded +/- local XYZ rotation in degrees. Y changes facing; Z rolls the card. Zero is exact alignment.");
    jitter["min"]=0; jitter["max"]=180;
    add("rock","mesh","Create a closed, seeded rock centered at the origin. Subdivided icosphere directions carry coherent radial weathering and optional horizontal strata. Use Boolean cuts for caves, cliffs and broken faces.",{
        {"size",boundedVec({1.5,1,1.2},.0001f,10000,"Nominal full XYZ dimensions in meters before noise; actual bounds vary with seed.")},
        {"subdivisions",integer(2,0,5,"Icosphere refinement: 20 * 4^subdivisions triangles. More detail resolves weathering; zero is a coarse icosahedron.")},
        {"roundness",number(.75f,.1f,1,"One starts from an ellipsoid; smaller values approach a block with rounded corners.")},
        {"noise",number(.24f,0,.7f,"Fractional radial weathering amplitude. Positive radii preserve a closed star-shaped surface.")},
        {"noise_scale",number(2.5f,.1f,32,"Noise frequency in normalized rock coordinates, independent of size.")},
        {"noise_octaves",integer(3,1,6,"Coherent weathering octaves, with halved amplitude at each octave.")},
        {"strata",number(0,0,.4f,"Fractional radial amplitude of horizontal layering.")},
        {"strata_frequency",number(5,1,32,"Layer cycles across nominal height. Increase subdivisions to resolve layers.")},
        {"shading",choice("flat",{"flat","smooth"},"Flat triangle facets or area-weighted smooth vertex normals. Both retain identical closed geometry.")},
        {"color_variation",number(.18f,0,1,"Coherent vertex-color darkening, multiplied by the material. Uses a separate seed stream from geometry.")},
        {"material",field("material","bark","Material slot; declare a stone material for rocks.")},{"seed",seed()}});
    auto crystalProfile=profile(0,4,"Polygon radius over normalized height. Each key makes a ring; only endpoints may be zero (a single apex). Positive endpoints are capped. Nonzero multipliers must be at least 0.0001, and at least one key must be positive.");
    crystalProfile["default"]={{0,1},{.72,1},{1,0}};
    add("crystal","mesh","Create a closed faceted prism or cut gem from polygon rings. Base is at Y=0 and top at height. Profiles form points, girdles, crowns and flat tables; flat normals preserve facets.",{
        {"radius",positive(.2f,"Circumradius in meters before the profile and jitter.")},
        {"height",positive(1,"Full height in meters, from base to tip or table.")},
        {"sides",integer(6,3,64,"Polygon sides per ring. Six suits quartz; eight supports octagonal gems.")},
        {"radius_profile",crystalProfile},
        {"radial_jitter",number(0,0,.35f,"Seeded fractional +/- radius variation per polygon corner, shared by all rings to retain planar prism faces.")},
        {"material",field("material","leaf","Material slot; declare gemstone colors and surface roughness.")},{"seed",seed()}});
    // JSON bounds use decimal doubles so documented endpoints also accept
    // recipes written with decimal literals, before conversion to float data.
    c["rock"]["parameters"]["roundness"]["min"]=.1;
    c["rock"]["parameters"]["noise_scale"]["min"]=.1;
    c["rock"]["parameters"]["noise"]["max"]=.7;
    c["crystal"]["parameters"]["radial_jitter"]["max"]=.35;
    add("orient","points","Aim prototype +Z (the card face normal) at a direction, optionally with seeded angular variation.",{
        {"input",ref({"points"})},{"mode",choice("outward",{"outward","inward","fixed","keep"},"Face away from center, toward center, a fixed direction, or keep incoming orientation.")},
        {"center",vec({0,0,0},"Pivot center for outward/inward facing.")},
        {"up",vec({0,1,0},"Nonzero preferred card-up axis.")},
        {"direction",vec({0,0,1},"Nonzero normal for fixed facing and fallback at the pivot.")},
        {"horizontal",field("boolean",false,"Project outward/inward direction perpendicular to up, keeping cards upright around the pivot.")},
        {"rotation_jitter",jitter},{"seed",seed()}});
    auto sizeJitter=vec({0,0,0},"Independent seeded fractional +/- local XYZ size variation around the prototype origin, before rotation. [0.1,0.15,0] varies width +/-10% and height +/-15%. UVs stay attached.");
    sizeJitter["min"]=0; sizeJitter["max"]=0.95;
    add("instance","mesh","Flatten source geometry onto placement frames. Sources can be branches, clusters, flowers or whole plants.",{
        {"input",ref({"mesh"})},{"points",ref({"points"})},{"scale",positive(1,"Additional uniform scale.")},
        {"scale_jitter",sizeJitter},
        {"density",number(1,0,1,"Keep ceil(density * placement count) copies as a nested seeded subset. Preserves original frame, size, color and atlas choice for survivors; zero removes all copies.")},
        {"atlas",field("string",nullptr,"TexUtil spritesheet JSON manifest, relative to recipe. Remap each copy's 0..1 UVs into one occupied cell; bind its atlas PNGs in the material.")},
        {"atlas_mode",choice("random",{"random","cycle","fixed"},"Seeded random cell, row-major cycling, or atlas_index. Requires atlas when non-default.")},
        {"atlas_index",integer(0,0,255,"Zero-based cell for fixed mode; must be occupied.")},
        {"rotation",vec({0,0,0},"Local XYZ Euler rotation in degrees before placement.")},
        {"color_variation",number(0.12f,0,1,"Per-instance vertex-color darkening; GLB preserves colors.")},{"seed",seed()}});
    add("ribbon","mesh","Sweep alpha-card strips along paths. A packed leafy-spray atlas can represent many leaves on each strip; crossed planes retain coverage from different views.",{
        {"input",ref({"skeleton"})},{"width",positive(.3f,"Full strip width in meters, independent of stem radius and atlas aspect ratio.")},
        {"density",number(1,0,1,"Retain ceil(density * path count) strips as nested seeded subsets; survivors keep their atlas cell, roll and color.")},
        {"planes",integer(2,1,8,"Crossed strips around each path tangent.")},{"stride",integer(4,1,512,"Retain every Nth path sample and both endpoints. Larger stride lowers topology.")},
        {"width_profile",profile(.001f,4,"Width multiplier over normalized retained arc length; positive widths keep end quads valid.")},
        {"rotation",number(0,-36000,36000,"Roll around the path tangent in degrees.")},{"rotation_jitter",number(0,0,180,"Seeded +/- roll for each path.")},
        {"atlas",field("string",nullptr,"TexUtil sprite atlas manifest. Each path uses one cell shared by all its crossed planes; bind its maps on the material.")},
        {"atlas_mode",choice("random",{"random","cycle","fixed"},"Select one occupied cell per path.")},{"atlas_index",integer(0,0,255,"Occupied cell index for fixed mode.")},
        {"color_variation",number(.12f,0,1,"Seeded per-path color darkening.")},{"material",field("material","leaf","Use double-sided MASK with an actual RGBA packed-spray texture.")},{"seed",seed()}});
    add("transform","same","Translate, rotate and uniformly scale paths, points or meshes.",{
        {"input",ref({"skeleton","points","mesh"})},{"translation",vec({0,0,0},"World translation in meters.")},
        {"rotation",vec({0,0,0},"XYZ Euler rotation in degrees, applied X then Y then Z.")},{"scale",positive(1,"Positive uniform scale; no mirroring.")}});
    add("merge","same","Combine two or more values of the same type, preserving mesh materials.",{
        {"inputs",{{"type","references"},{"required",true},{"description","2..128 upstream node names, all of the same kind."}}}});
    add("solidify","mesh","Boolean-union the closed shells in a mesh, removing internal faces and fusing intersecting wood. Preserves material/UV/color/wind data and recalculates normals. Open leaves/cards must be added afterward.",{
        {"input",ref({"mesh"})},
        {"weld_tolerance",number(0,0,0.1f,"World-meter tolerance for joining duplicated seam vertices. Zero uses a scale-dependent float-precision tolerance; not a gap-filling radius.")},
        {"crease_angle",number(180,0,180,"Normal-smoothing threshold in degrees. 180 smooths shading across joins; lower values keep sharp creases. Does not move the surface.")},
        {"require_connected",field("boolean",true,"Fail unless union produces exactly one connected closed solid. False permits multiple disconnected solids, without adding bridges.")},
        {"max_input_triangles",integer(250000,4,1000000,"Bound source triangles before preparing the boolean input. Output still uses the global geometry budget.")},
        {"max_shells",integer(1024,1,10000,"Bound closed input shells before running the union.")}});
    auto booleanFields=c["solidify"]["parameters"];
    booleanFields["tool"]=ref({"mesh"});
    booleanFields["operation"]=choice("difference",{"union","difference","intersection"},"Combine input and tool: union, input minus tool, or their common volume. Use solidify first on overlapping shell assemblies. Tool materials label newly exposed cuts.");
    booleanFields["crease_angle"]["default"]=45;
    booleanFields["max_input_triangles"]["description"]="Bound total triangles across both operands before preparing Boolean inputs.";
    booleanFields["max_shells"]["description"]="Bound total input boundary shells, including cavity surfaces, before the Boolean operation.";
    booleanFields["require_connected"]["description"]="Require at most one positive-volume result body. Closed cavities are allowed. Empty intermediate results are allowed; an empty final output fails ordinary export.";
    add("boolean","mesh","Union, subtract or intersect two oriented closed solids. Preserves source materials, UVs, colors and wind, including cutter properties on new cuts. Recomputes normals; permits cavities and chained cuts.",booleanFields);
    add("material","mesh","Assign one material to all faces of a mesh.",{{"input",ref({"mesh"})},{"name",field("material","leaf","Material slot name.")}});
    add("wind","mesh","Write normalized height bending weight and stable phase into GLB custom _WIND attribute.",{
        {"input",ref({"mesh"})},{"base",number(0,-100000,100000,"World Y where bending weight starts.")},
        {"height",positive(4,"Height interval for weight 0..1.")},{"exponent",number(1.5f,0.01f,16,"Weight curve exponent.")},
        {"strength",number(1,0,1,"Maximum bending weight. This is metadata, not a wind simulation.")},{"seed",seed()}});
    add("mesh","mesh","Import an OBJ prototype with positions, optional UVs/normals, and convex polygon faces.",{
        {"path",{{"type","string"},{"required",true},{"description","OBJ filename relative to recipe. Material slots are replaced by material."}}},
        {"material",field("material","leaf","Material slot for imported faces.")}});
    return c;
}
Json materialFields() {
    return {{"base_color",field("color",{0.35,0.5,0.16,1},"Linear RGBA, multiplied by vertex color and optional sRGB texture.")},
        {"roughness",number(0.8f,0,1,"PBR roughness multiplier.")},{"metallic",number(0,0,1,"PBR metallic multiplier.")},
        {"transmission",number(0,0,1,"Optical transmission through glass or gems. Exports KHR_materials_transmission; independent of alpha coverage and thin-tissue translucency. Requires a compatible viewer shader.")},
        {"ior",number(1.5f,1,3,"Dielectric index of refraction, exported with KHR_materials_ior when non-default. E.g. diamond 2.42. Does not model dispersion.")},
        {"thickness",number(0,0,10000,"Approximate volume thickness in mesh meters for refractive GLB materials. Positive values require transmission and a closed mesh; ray tracers use the actual surface distance.")},
        {"attenuation_color",boundedVec({1,1,1},0,1,"Linear RGB light remaining after attenuation_distance through a transmissive volume.")},
        {"attenuation_distance",{{"type","number"},{"min",.000001},{"max",1000000},{"description","Absorption distance in world meters for volume transmission. Omit for no volume absorption. Requires positive thickness."}}},
        {"translucency",number(0,0,1,"Thin-surface diffuse transmission weight, separate from alpha coverage. Saved in GLB foliageutil extras; applied by the Blender preview adapter.")},
        {"translucency_color",boundedVec({1,1,1},0,1,"Linear RGB transmission tint, multiplied by textured base color in the Blender preview.")},
        {"subsurface",number(0,0,1,"Burley surface diffusion weight in the Blender preview. Saved in GLB foliageutil extras.")},
        {"subsurface_scale",number(0.0015f,0.000001f,1,"Surface diffusion scale in meters in the Blender preview.")},
        {"subsurface_radius",boundedVec({1,0.45,0.25},0.000001f,16,"Positive RGB scattering radius ratios, multiplied by subsurface_scale.")},
        {"double_sided",field("boolean",true,"Disable back-face culling for thin leaves/cards.")},
        {"alpha_mode",choice("OPAQUE",{"OPAQUE","MASK","BLEND"},"glTF coverage mode.")},{"alpha_cutoff",number(0.5f,0,1,"MASK cutoff.")},
        {"base_color_texture",field("string",nullptr,"PNG or JPEG, relative to recipe; embedded in GLB.")},
        {"normal_texture",field("string",nullptr,"Tangent-space OpenGL normal PNG/JPEG.")},
        {"metallic_roughness_texture",field("string",nullptr,"Linear packed texture, roughness in G, metallic in B.")}};
}
void validateFields(Json& object, const Json& fields, const std::string& context) {
    require(object.is_object(),context+" must be an object");
    for(auto it=object.begin();it!=object.end();++it) require(fields.contains(it.key()),context+": unknown parameter '"+it.key()+"'");
    for(auto it=fields.begin();it!=fields.end();++it) {
        const auto& rule=it.value(); const std::string label=context+"."+it.key();
        if(!object.contains(it.key())) {
            require(!rule.value("required",false),label+" is required");
            if(rule.contains("default")&&!rule["default"].is_null()) object[it.key()]=rule["default"];
            continue;
        }
        const auto& v=object[it.key()]; std::string type=rule.at("type");
        auto numeric=[&](const Json& n) { require(n.is_number()&&std::isfinite(n.get<double>()),label+" must contain finite numbers"); if(rule.contains("min")) require(n.get<double>()>=rule["min"].get<double>()&&n.get<double>()<=rule["max"].get<double>(),label+" is outside allowed bounds"); };
        if(type=="number"||type=="integer") { numeric(v); if(type=="integer") require(v.is_number_integer(),label+" must be an integer"); }
        else if(type=="boolean") require(v.is_boolean(),label+" must be boolean");
        else if(type=="string"||type=="reference"||type=="material") {
            require(v.is_string()&&!v.get<std::string>().empty()&&v.get<std::string>().size()<=1024,label+" must be a nonempty string up to 1024 bytes");
            const auto s=v.get<std::string>(); require(s.find('\0')==std::string::npos&&s.find('\n')==std::string::npos&&s.find('\r')==std::string::npos,label+" contains a control character");
            if(type=="material") require(s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos,label+" must use letters, digits, _ or -");
        } else if(type=="pair"||type=="vec3"||type=="color"||type=="rect") {
            size_t n=type=="pair"?2:type=="vec3"?3:4;
            require(v.is_array()&&v.size()==n,label+" has wrong vector size"); for(const auto& x:v) numeric(x);
            if(type=="color"||type=="rect") for(const auto& x:v) require(x.get<double>()>=0&&x.get<double>()<=1,label+" must be within 0..1");
            if(type=="rect") require(v[0]<v[2]&&v[1]<v[3],label+" must have positive width and height");
        } else if(type=="profile") {
            require(v.is_array()&&v.size()>=2&&v.size()<=32,label+" needs 2..32 [position, multiplier] keys");
            double previous=-1;
            for(const auto& key:v) {
                require(key.is_array()&&key.size()==2,label+" needs [position, multiplier] keys");
                require(key[0].is_number()&&std::isfinite(key[0].get<double>()),label+" positions must be finite numbers");
                double position=key[0].get<double>();
                require(position>=0&&position<=1&&position>previous,label+" positions must increase strictly within 0..1");
                require(previous<0||key[0].get<float>()>float(previous),label+" positions must remain distinct at float precision");
                numeric(key[1]); previous=position;
            }
            require(v.front()[0]==0&&v.back()[0]==1,label+" must start at 0 and end at 1");
        } else if(type=="references") {
            require(v.is_array()&&v.size()>=2&&v.size()<=128,label+" needs 2..128 references");
            for(const auto& x:v) require(x.is_string()&&!x.get<std::string>().empty(),label+" needs node names");
        } else if(type=="positions") {
            require(v.is_array()&&v.size()>=2&&v.size()<=513,label+" needs 2..513 points");
            for(const auto& p:v) { require(p.is_array()&&p.size()==3,label+" needs XYZ positions"); for(const auto& x:p) { numeric(x); require(std::abs(x.get<double>())<=100000,label+" coordinate exceeds 100000"); } }
            for(size_t i=1;i<v.size();++i) require(length(vector3(v[i])-vector3(v[i-1]))>1e-7f,label+" has coincident consecutive points");
        } else throw std::logic_error("unknown field type "+type);
        if(rule.contains("enum")) require(std::find(rule["enum"].begin(),rule["enum"].end(),v)!=rule["enum"].end(),label+" has an unknown enum value");
    }
}
}
